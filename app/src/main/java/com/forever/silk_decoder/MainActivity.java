package com.forever.silk_decoder;

import android.Manifest;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    Button button;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        ActivityCompat.requestPermissions(this, new String[]{
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE}, 100);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
//        tv.setText(getString("ssssss----"));
        button = findViewById(R.id.test);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                String ret = getDecoder("/data/data/com.forever.silk_decoder/test.amr");
                Toast.makeText(getBaseContext(), ret, Toast.LENGTH_SHORT).show();
            }
        });
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */

    public native String getDecoder(String str);

}