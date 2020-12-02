#include <jni.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <android/log.h>
#include "include/SKP_Silk_SDK_API.h"
#include "SKP_Silk_SigProc_FIX.h"

/* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2


#define TAG "SilkDecoder"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG ,__VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG ,__VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG ,__VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG ,__VA_ARGS__)

#ifdef _SYSTEM_IS_BIG_ENDIAN
/* Function to convert a little endian int16 to a */
/* big endian int16 or vica verca                 */
void swap_endian(
    SKP_int16       vec[],
    SKP_int         len
)
{
    SKP_int i;
    SKP_int16 tmp;
    SKP_uint8 *p1, *p2;

    for( i = 0; i < len; i++ ){
        tmp = vec[ i ];
        p1 = (SKP_uint8 *)&vec[ i ]; p2 = (SKP_uint8 *)&tmp;
        p1[ 0 ] = p2[ 1 ]; p1[ 1 ] = p2[ 0 ];
    }
}
#endif

unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((tv.tv_sec * 1000000) + (tv.tv_usec));
}

static SKP_int32 rand_seed = 1;

static char* pcmToWav(char *pcmPath, int chanel, int sampleRate, int bitsPerSample);

static bool isEmpty(char* str) {
    return (strlen(str) == 0);
}

static jstring cstr2Jstring( JNIEnv* env, char* pat )
{
    jclass strClass = (env)->FindClass("java/lang/String");
    jmethodID ctorID = (env)->GetMethodID(strClass, "<init>","([BLjava/lang/String;)V");
    jbyteArray bytes = (env)->NewByteArray(strlen(pat));
    (env)->SetByteArrayRegion(bytes, 0, strlen(pat), (jbyte *) pat);
    jstring encoding = (env)->NewStringUTF("GB2312");
    return (jstring) (env)->NewObject(strClass, ctorID, bytes, encoding);
}

static char * jstring2CStr( JNIEnv * env, jstring jstr )
{
    char * rtn = NULL;
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("GB2312");
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr= (jbyteArray)env->CallObjectMethod(jstr,mid,strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte * ba = env->GetByteArrayElements(barr,JNI_FALSE);
    if(alen > 0)
    {
        rtn = (char*)malloc(alen+1); //new char[alen+1];
        memcpy(rtn,ba,alen);
        rtn[alen]=0;
    }
    env->ReleaseByteArrayElements(barr,ba,0);

    return rtn;
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_forever_silk_1decoder_MainActivity_getDecoder(JNIEnv *env, jobject /* this */, jstring filePath) {
    unsigned long tottime, starttime;
    double filetime;
    size_t counter;
    SKP_int32 totPackets, i, k;
    SKP_int16 ret, len, tot_len;
    SKP_int16 nBytes;
    SKP_uint8 payload[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * (MAX_LBRR_DELAY + 1)];
    SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
    SKP_uint8 FECpayload[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES], *payloadPtr;
    SKP_int16 nBytesFEC;
    SKP_int16 nBytesPerPacket[MAX_LBRR_DELAY + 1], totBytes;
    SKP_int16 out[((FRAME_LENGTH_MS * MAX_API_FS_KHZ) << 1) * MAX_INPUT_FRAMES], *outPtr;
    char speechOutFileName[200], bitInFileName[200];
    FILE *bitInFile, *speechOutFile;
    SKP_int32 packetSize_ms = 0, API_Fs_Hz = 0;
    SKP_int32 decSizeBytes;
    void *psDec;
    SKP_float loss_prob;
    SKP_int32 frames, lost, quiet;
    SKP_SILK_SDK_DecControlStruct DecControl;

    /* default settings */
    quiet = 0;
    loss_prob = 0.0f;
    /* get arguments */
    memset(bitInFileName, '\0', sizeof(bitInFileName));
    memset(speechOutFileName, '\0', sizeof(speechOutFileName));
    strcpy(bitInFileName, jstring2CStr(env, filePath));
    strncat(speechOutFileName, bitInFileName, strlen(bitInFileName) - 3);
    strcat(speechOutFileName, "pcm");
    ALOGI("[start]: %s -> %s", bitInFileName, speechOutFileName);

    if (!quiet) {
        ALOGI("********** Silk Decoder (Fixed Point) v %s ********************\n",
               SKP_Silk_SDK_get_version());
        ALOGI("********** Compiled for %d bit cpu *******************************\n",
               (int) sizeof(void *) * 8);
        ALOGI("Input:                       %s\n", bitInFileName);
        ALOGI("Output:                      %s\n", speechOutFileName);
    }

    /* Open files */
    bitInFile = fopen(bitInFileName, "rb");
    if (bitInFile == NULL) {
        ALOGE("Error: could not open input file %s\n", bitInFileName);
        return cstr2Jstring(env, "");
    }

    /* Check Silk header */
    {
        char header_buf[50];
        fread(header_buf, sizeof(char), 1, bitInFile);
        header_buf[strlen("")] = '\0'; /* Terminate with a null character */
        if (strcmp(header_buf, "") != 0) {
            counter = fread(header_buf, sizeof(char), strlen("!SILK_V3"), bitInFile);
            header_buf[strlen("!SILK_V3")] = '\0'; /* Terminate with a null character */
            if (strcmp(header_buf, "!SILK_V3") != 0) {
                /* Non-equal strings */
                ALOGE("Error: Wrong Header %s\n", header_buf);
                return cstr2Jstring(env, "");
            }
        } else {
            counter = fread(header_buf, sizeof(char), strlen("#!SILK_V3"), bitInFile);
            header_buf[strlen("#!SILK_V3")] = '\0'; /* Terminate with a null character */
            if (strcmp(header_buf, "#!SILK_V3") != 0) {
                /* Non-equal strings */
                ALOGE("Error: Wrong Header %s\n", header_buf);
                return cstr2Jstring(env, "");
            }
        }
    }

    speechOutFile = fopen(speechOutFileName, "wb");
    if (speechOutFile == NULL) {
        ALOGE("Error: could not open output file %s\n", speechOutFileName);
        return cstr2Jstring(env, "");
    }

    /* Set the samplingrate that is requested for the output */
    if (API_Fs_Hz == 0) {
        DecControl.API_sampleRate = 24000;
    } else {
        DecControl.API_sampleRate = API_Fs_Hz;
    }

    /* Initialize to one frame per packet, for proper concealment before first packet arrives */
    DecControl.framesPerPacket = 1;

    /* Create decoder */
    ret = SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes);
    if (ret) {
        ALOGI("\nSKP_Silk_SDK_Get_Decoder_Size returned %d", ret);
    }
    psDec = malloc(decSizeBytes);

    /* Reset decoder */
    ret = SKP_Silk_SDK_InitDecoder(psDec);
    if (ret) {
        ALOGI("\nSKP_Silk_InitDecoder returned %d", ret);
    }

    totPackets = 0;
    tottime = 0;
    payloadEnd = payload;

    /* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
    for (i = 0; i < MAX_LBRR_DELAY; i++) {
        /* Read payload size */
        counter = fread(&nBytes, sizeof(SKP_int16), 1, bitInFile);
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes, 1 );
#endif
        /* Read payload */
        counter = fread(payloadEnd, sizeof(SKP_uint8), nBytes, bitInFile);

        if ((SKP_int16) counter < nBytes) {
            break;
        }
        nBytesPerPacket[i] = nBytes;
        payloadEnd += nBytes;
        totPackets++;
    }

    while (1) {
        /* Read payload size */
        counter = fread(&nBytes, sizeof(SKP_int16), 1, bitInFile);
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes, 1 );
#endif
        if (nBytes < 0 || counter < 1) {
            break;
        }

        /* Read payload */
        counter = fread(payloadEnd, sizeof(SKP_uint8), nBytes, bitInFile);
        if ((SKP_int16) counter < nBytes) {
            break;
        }

        /* Simulate losses */
        rand_seed = SKP_RAND(rand_seed);
        if ((((float) ((rand_seed >> 16) + (1 << 15))) / 65535.0f >= (loss_prob / 100.0f)) &&
            (counter > 0)) {
            nBytesPerPacket[MAX_LBRR_DELAY] = nBytes;
            payloadEnd += nBytes;
        } else {
            nBytesPerPacket[MAX_LBRR_DELAY] = 0;
        }

        if (nBytesPerPacket[0] == 0) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for (i = 0; i < MAX_LBRR_DELAY; i++) {
                if (nBytesPerPacket[i + 1] > 0) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR(payloadPtr, nBytesPerPacket[i + 1], (i + 1),
                                                 FECpayload, &nBytesFEC);
                    tottime += GetHighResolutionTime() - starttime;
                    if (nBytesFEC > 0) {
                        payloadToDec = FECpayload;
                        nBytes = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[i + 1];
            }
        } else {
            lost = 0;
            nBytes = nBytesPerPacket[0];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if (lost == 0) {
            /* No Loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 0, payloadToDec, nBytes, outPtr,
                                          &len);
                if (ret) {
                    ALOGI("\nSKP_Silk_SDK_Decode returned %d", ret);
                }

                frames++;
                outPtr += len;
                tot_len += len;
                if (frames > MAX_INPUT_FRAMES) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr = out;
                    tot_len = 0;
                    frames = 0;
                }
                /* Until last 20 ms frame of packet has been decoded */
            } while (DecControl.moreInternalDecoderFrames);
        } else {
            /* Loss: Decode enough frames to cover one packet duration */
            for (i = 0; i < DecControl.framesPerPacket; i++) {
                /* Generate 20 ms */
                ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 1, payloadToDec, nBytes, outPtr,
                                          &len);
                if (ret) {
                    ALOGI("\nSKP_Silk_Decode returned %d", ret);
                }
                outPtr += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / (DecControl.API_sampleRate / 1000);
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        fwrite(out, sizeof(SKP_int16), tot_len, speechOutFile);

        /* Update buffer */
        totBytes = 0;
        for (i = 0; i < MAX_LBRR_DELAY; i++) {
            totBytes += nBytesPerPacket[i + 1];
        }
        /* Check if the received totBytes is valid */
        if (totBytes < 0 || totBytes > sizeof(payload)) {
            fprintf(stderr, "\rPackets decoded:             %d", totPackets);
            return cstr2Jstring(env, "");
        }
        SKP_memmove(payload, &payload[nBytesPerPacket[0]], totBytes * sizeof(SKP_uint8));
        payloadEnd -= nBytesPerPacket[0];
        SKP_memmove(nBytesPerPacket, &nBytesPerPacket[1], MAX_LBRR_DELAY * sizeof(SKP_int16));

        if (!quiet) {
            fprintf(stderr, "\rPackets decoded:             %d", totPackets);
        }
    }

    /* Empty the recieve buffer */
    for (k = 0; k < MAX_LBRR_DELAY; k++) {
        if (nBytesPerPacket[0] == 0) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for (i = 0; i < MAX_LBRR_DELAY; i++) {
                if (nBytesPerPacket[i + 1] > 0) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR(payloadPtr, nBytesPerPacket[i + 1], (i + 1),
                                                 FECpayload, &nBytesFEC);
                    tottime += GetHighResolutionTime() - starttime;
                    if (nBytesFEC > 0) {
                        payloadToDec = FECpayload;
                        nBytes = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[i + 1];
            }
        } else {
            lost = 0;
            nBytes = nBytesPerPacket[0];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if (lost == 0) {
            /* No loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 0, payloadToDec, nBytes, outPtr,
                                          &len);
                if (ret) {
                    ALOGI("\nSKP_Silk_SDK_Decode returned %d", ret);
                }

                frames++;
                outPtr += len;
                tot_len += len;
                if (frames > MAX_INPUT_FRAMES) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr = out;
                    tot_len = 0;
                    frames = 0;
                }
                /* Until last 20 ms frame of packet has been decoded */
            } while (DecControl.moreInternalDecoderFrames);
        } else {
            /* Loss: Decode enough frames to cover one packet duration */

            /* Generate 20 ms */
            for (i = 0; i < DecControl.framesPerPacket; i++) {
                ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 1, payloadToDec, nBytes, outPtr,
                                          &len);
                if (ret) {
                    ALOGI("\nSKP_Silk_Decode returned %d", ret);
                }
                outPtr += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / (DecControl.API_sampleRate / 1000);
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        fwrite(out, sizeof(SKP_int16), tot_len, speechOutFile);

        /* Update Buffer */
        totBytes = 0;
        for (i = 0; i < MAX_LBRR_DELAY; i++) {
            totBytes += nBytesPerPacket[i + 1];
        }

        /* Check if the received totBytes is valid */
        if (totBytes < 0 || totBytes > sizeof(payload)) {
            fprintf(stderr, "\rPackets decoded:              %d", totPackets);
            return cstr2Jstring(env, "");
        }

        SKP_memmove(payload, &payload[nBytesPerPacket[0]], totBytes * sizeof(SKP_uint8));
        payloadEnd -= nBytesPerPacket[0];
        SKP_memmove(nBytesPerPacket, &nBytesPerPacket[1], MAX_LBRR_DELAY * sizeof(SKP_int16));

        if (!quiet) {
            fprintf(stderr, "\rPackets decoded:              %d", totPackets);
        }
    }

    if (!quiet) {
        ALOGI("Decoding Finished");
    }

    /* Free decoder */
    free(psDec);

    /* Close files */
    fclose(speechOutFile);
    fclose(bitInFile);

    //pcm to wav
    char wavPath[200];
    strcpy(wavPath, pcmToWav(speechOutFileName, 1, 24000, 16));
    ALOGI("[Decode Success]: %s", wavPath);
    if (isEmpty(wavPath)) {
        ALOGE("failed in pcm to wav");
        return cstr2Jstring(env, "");
    }

    filetime = totPackets * 1e-3 * packetSize_ms;
    if (!quiet) {
        ALOGI("File length:                 %.3f s", filetime);
        ALOGI("Time for decoding:           %.3f s (%.3f%% of realtime)", 1e-6 * tottime,
              1e-4 * tottime / filetime);
    } else {
        /* print time and % of realtime */
        ALOGI("%.3f %.3f %d\n", 1e-6 * tottime, 1e-4 * tottime / filetime, totPackets);
    }
    return cstr2Jstring(env, wavPath);
}

//pcm to wav
typedef unsigned char ID[4];

typedef struct
{
    ID          chunkID;  /* {'f', 'm', 't', ' '} */
    uint32_t    chunkSize;

    uint16_t    audioFormat;
    uint16_t    numChannels;
    uint32_t    sampleRate;
    uint32_t    byteRate;
    uint16_t    blockAlign;
    uint16_t    bitsPerSample;
} FormatChunk;

typedef struct
{
    ID             chunkID;  /* {'d', 'a', 't', 'a'}  */
    uint32_t       chunkSize;
    unsigned char  data[];
} DataChunk;

// path 1 24000 16
static char* pcmToWav(char *pcmPath, int chanel, int sampleRate, int bitsPerSample) {
    char wavPath[200];
    memset(wavPath, '\0', sizeof(wavPath));
    strncat(wavPath, pcmPath, strlen(pcmPath) - 3);
    strcat(wavPath, "wav");
    ALOGI("[pcm to wav] %s -> %s", pcmPath, wavPath);

    FILE *pcmFile, *wavFile;
    uint32_t  pcmfile_size, chunk_size;
    FormatChunk formatchunk;
    DataChunk   datachunk;
    int read_len;
    char buf[1024];

    pcmFile = fopen(pcmPath, "rb");
    if (pcmFile == NULL) {
        ALOGE("Can't open pcmfile: %s", pcmPath);
        return "";
    }
    fseek(pcmFile, 0, SEEK_END);
    pcmfile_size = ftell(pcmFile);
    fseek(pcmFile, 0, SEEK_SET);

    wavFile = fopen(wavPath, "wb");
    if (wavFile == NULL) {
        ALOGE("Can't create wavfile: %s", wavPath);
        return "";
    }

    fwrite("RIFF", 1, 4, wavFile);
    fwrite("xxxx", 1, 4, wavFile);  //reserved for the total chunk size
    fwrite("WAVE", 1, 4, wavFile);
    formatchunk.chunkID[0] = 'f';
    formatchunk.chunkID[1] = 'm';
    formatchunk.chunkID[2] = 't';
    formatchunk.chunkID[3] = ' ';
    formatchunk.chunkSize  = 16;
    formatchunk.audioFormat = 1;
    formatchunk.numChannels = chanel;
    formatchunk.sampleRate = sampleRate;
    formatchunk.bitsPerSample = bitsPerSample;
    formatchunk.byteRate = formatchunk.sampleRate * formatchunk.numChannels * (formatchunk.bitsPerSample >> 3);
    formatchunk.blockAlign = formatchunk.numChannels * (formatchunk.bitsPerSample >> 3);
    fwrite(&formatchunk, 1, sizeof(formatchunk), wavFile);

    datachunk.chunkID[0] = 'd';
    datachunk.chunkID[1] = 'a';
    datachunk.chunkID[2] = 't';
    datachunk.chunkID[3] = 'a';
    datachunk.chunkSize = pcmfile_size;
    fwrite(&datachunk, 1, sizeof(ID)+sizeof(uint32_t), wavFile);

    while((read_len = fread(buf, 1, sizeof(buf), pcmFile)) != 0) {
        fwrite(buf, 1, read_len, wavFile);
    }

    fseek(wavFile, 4, SEEK_SET);
    chunk_size = 4 + (8 + formatchunk.chunkSize) + (8 + datachunk.chunkSize);
    fwrite(&chunk_size, 1, 4, wavFile);

    fclose(pcmFile);
    fclose(pcmFile);
    remove(pcmPath);
    return wavPath;
}



