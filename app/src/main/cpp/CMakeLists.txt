# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

# Declares and names the project.

project("silk_decoder")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        native-lib

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp
        SKP_Silk_A2NLSF.c
        SKP_Silk_decode_frame.c
        SKP_Silk_array_maxabs.c
        SKP_Silk_dec_API.c
        SKP_Silk_range_coder.c
        SKP_Silk_resampler.c
        SKP_Silk_resampler_up2.c
        SKP_Silk_resampler_private_up4.c
        SKP_Silk_resampler_private_IIR_FIR.c
        SKP_Silk_resampler_private_down_FIR.c
        SKP_Silk_resampler_down2.c
        SKP_Silk_resampler_private_down4.c
        SKP_Silk_decode_parameters.c
        SKP_Silk_decode_core.c
        SKP_Silk_biquad.c
        SKP_Silk_create_init_destroy.c
        SKP_Silk_bwexpander_32.c
        SKP_Silk_PLC.c
        SKP_Silk_decoder_set_fs.c
        SKP_Silk_tables_other.c
        SKP_Silk_sum_sqr_shift.c
        SKP_Silk_CNG.c
        SKP_Silk_bwexpander.c
        SKP_Silk_LPC_inv_pred_gain.c
        SKP_Silk_NLSF2A_stable.c
        SKP_Silk_resampler_private_ARMA4.c
        SKP_Silk_resampler_rom.c
        SKP_Silk_resampler_private_up2_HQ.c
        SKP_Silk_resampler_private_copy.c
        SKP_Silk_LPC_synthesis_order16.c
        SKP_Silk_LPC_synthesis_filter.c
        SKP_Silk_decode_pulses.c
        SKP_Silk_shell_coder.c
        SKP_Silk_code_signs.c
        SKP_Silk_tables_type_offset.c
        SKP_Silk_tables_pulses_per_block.c
        SKP_Silk_NLSF2A.c
        SKP_Silk_tables_sign.c
        SKP_Silk_MA.c
        SKP_Silk_LSF_cos_table.c
        SKP_Silk_tables_LTP.c
        SKP_Silk_decode_pitch.c
        SKP_Silk_pitch_est_tables.c
        SKP_Silk_tables_pitch_lag.c
        SKP_Silk_NLSF_MSVQ_decode.c
        SKP_Silk_gain_quant.c
        SKP_Silk_NLSF_stabilize.c
        SKP_Silk_lin2log.c
        SKP_Silk_log2lin.c
        SKP_Silk_sort.c
        SKP_Silk_tables_NLSF_CB0_10.c
        SKP_Silk_tables_NLSF_CB1_10.c
        SKP_Silk_tables_NLSF_CB0_16.c
        SKP_Silk_tables_NLSF_CB1_16.c
        SKP_Silk_tables_gain.c
        SKP_Silk_resampler_private_AR2.c)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        native-lib

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})