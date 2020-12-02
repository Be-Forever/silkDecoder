package com.forever.silk_decoder;

public class Decoder {
    static {
        System.loadLibrary("native-lib-1");
    }

    public static native String getDecoder(String str);
}
