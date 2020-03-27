package com.github.chagall.notificationlistenerexample;

import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.ParcelUuid;
import android.provider.Settings;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Set;

/**
 * MIT License
 *
 *  Copyright (c) 2016 Fábio Alves Martins Pereira (Chagall)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String ENABLED_NOTIFICATION_LISTENERS = "enabled_notification_listeners";
    private static final String ACTION_NOTIFICATION_LISTENER_SETTINGS = "android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS";

    private ImageView interceptedNotificationImageView;
    private ImageChangeBroadcastReceiver imageChangeBroadcastReceiver;
    private AlertDialog enableNotificationListenerAlertDialog;

    private static OutputStream outputStream;
    private InputStream inStream;
    private BluetoothSocket socket;
    private TextView textView;
    private TextView hour;
    private SeekBar h_seek;
    private SeekBar m_seek;
    private int h;
    private int m;

    private void init() throws IOException {
        BluetoothAdapter blueAdapter = BluetoothAdapter.getDefaultAdapter();
        if (blueAdapter != null) {
            if (blueAdapter.isEnabled()) {
                Set<BluetoothDevice> bondedDevices = blueAdapter.getBondedDevices();

                for (BluetoothDevice bd : bondedDevices) {
                    if (bd.getName().contains("HC-05")) {
                        Toast.makeText(this, "Connected", Toast.LENGTH_LONG).show();
                        socket = bd.createRfcommSocketToServiceRecord(bd.getUuids()[0].getUuid());
                        socket.connect();
                        outputStream = socket.getOutputStream();
                        inStream = socket.getInputStream();
                    }
                }
            }
        }
    }

    public static void write(String s) throws IOException {
        outputStream.write(s.getBytes());
    }

    public void read() {
        final int BUFFER_SIZE = 30;
        byte[] buffer = new byte[BUFFER_SIZE];
        int bytes = 0;

        try {
            inStream.read(buffer, bytes, BUFFER_SIZE - bytes);
            StringBuilder output = new StringBuilder();
            String str = new String(buffer).substring(1, new String(buffer).indexOf('}'));
            String temp = str.substring(0, str.indexOf(","));
            String humid = str.substring(str.indexOf(",") + 1);

            output.append("Temp: ").append(temp.substring(0, temp.length() - 1)).append(',').append(temp.substring(temp.length() - 1)).append(" °C\n");
            output.append("Humid: ").append(humid.substring(0, humid.length() - 1)).append(',').append(humid.substring(humid.length() - 1)).append(" %\n");
            textView.setText(output);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onBackPressed() {
        try {
            socket.close();
            super.onBackPressed();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        try {
            init();
        } catch (IOException e) {
            e.printStackTrace();
        }

        textView = (TextView) findViewById(R.id.info);
        hour = (TextView) findViewById(R.id.hour);
        hour.setText("Alarm at 0:0");
        h_seek = (SeekBar) findViewById(R.id.h_seek);
        m_seek = (SeekBar) findViewById(R.id.m_seek);

        h_seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                h = (i * 23) / 100;
                String out = "Alarm at " + h + ":" + m;
                hour.setText(out);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        m_seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                m = (i * 59) / 100;
                String out = "Alarm at " + h + ":" + m;
                hour.setText(out);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        // Here we get a reference to the image we will modify when a notification is received
        interceptedNotificationImageView
                = (ImageView) this.findViewById(R.id.intercepted_notification_logo);

        // If the user did not turn the notification listener service on we prompt him to do so
        if(!isNotificationServiceEnabled()){
            enableNotificationListenerAlertDialog = buildNotificationServiceAlertDialog();
            enableNotificationListenerAlertDialog.show();
        }

        // Finally we register a receiver to tell the MainActivity when a notification has been received
        imageChangeBroadcastReceiver = new ImageChangeBroadcastReceiver();
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction("com.github.chagall.notificationlistenerexample");
        registerReceiver(imageChangeBroadcastReceiver, intentFilter);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(imageChangeBroadcastReceiver);
    }

    /**
     * Change Intercepted Notification Image
     * Changes the MainActivity image based on which notification was intercepted
     * @param notificationCode The intercepted notification code
     */
    private void changeInterceptedNotificationImage(int notificationCode){
        switch(notificationCode){
            case NotificationListenerExampleService.InterceptedNotificationCode.FACEBOOK_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.facebook_logo);
                break;
            case NotificationListenerExampleService.InterceptedNotificationCode.INSTAGRAM_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.instagram_logo);
                break;
            case NotificationListenerExampleService.InterceptedNotificationCode.WHATSAPP_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.whatsapp_logo);
                break;
            case NotificationListenerExampleService.InterceptedNotificationCode.SNAPCHAT_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.snapchat_logo);
                break;
            case NotificationListenerExampleService.InterceptedNotificationCode.MESSAGES_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.message_logo);
                break;
            case NotificationListenerExampleService.InterceptedNotificationCode.CALL_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.call_logo);
                break;
            case NotificationListenerExampleService.InterceptedNotificationCode.OTHER_NOTIFICATIONS_CODE:
                interceptedNotificationImageView.setImageResource(R.drawable.other_notification_logo);
                break;
        }
    }

    /**
     * Is Notification Service Enabled.
     * Verifies if the notification listener service is enabled.
     * Got it from: https://github.com/kpbird/NotificationListenerService-Example/blob/master/NLSExample/src/main/java/com/kpbird/nlsexample/NLService.java
     * @return True if eanbled, false otherwise.
     */
    private boolean isNotificationServiceEnabled(){
        String pkgName = getPackageName();
        final String flat = Settings.Secure.getString(getContentResolver(),
                ENABLED_NOTIFICATION_LISTENERS);
        if (!TextUtils.isEmpty(flat)) {
            final String[] names = flat.split(":");
            for (int i = 0; i < names.length; i++) {
                final ComponentName cn = ComponentName.unflattenFromString(names[i]);
                if (cn != null) {
                    if (TextUtils.equals(pkgName, cn.getPackageName())) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.sync:
                try {
                    write("{sync}");
                    new Handler().postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            read();
                        }
                    }, 5000);
                } catch (IOException e) {
                    e.printStackTrace();
                }
                break;
            case R.id.set:
                try {
                    write("{alarm:" + h + ":" + m + "}");
                    Toast.makeText(this, "Alarm was set for " + h + ":" + m, Toast.LENGTH_LONG).show();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                break;
            case R.id.reset:
                try {
                    write("{reset}");
                    Toast.makeText(this, "Alarm was reset", Toast.LENGTH_LONG).show();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                break;
            case R.id.scroll:
                try {
                    write("{scroll}");
                    Toast.makeText(this, "Scrolled down", Toast.LENGTH_LONG).show();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                break;
            case R.id.change:
                try {
                    write("{change}");
                    Toast.makeText(this, "Screen was changed", Toast.LENGTH_LONG).show();
                } catch (IOException e) {
                    e.printStackTrace();
                }
                break;
        }
    }

    /**
     * Image Change Broadcast Receiver.
     * We use this Broadcast Receiver to notify the Main Activity when
     * a new notification has arrived, so it can properly change the
     * notification image
     * */
    public class ImageChangeBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int receivedNotificationCode = intent.getIntExtra("Notification Code",-1);
            changeInterceptedNotificationImage(receivedNotificationCode);
        }
    }


    /**
     * Build Notification Listener Alert Dialog.
     * Builds the alert dialog that pops up if the user has not turned
     * the Notification Listener Service on yet.
     * @return An alert dialog which leads to the notification enabling screen
     */
    private AlertDialog buildNotificationServiceAlertDialog(){
        AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(this);
        alertDialogBuilder.setTitle(R.string.notification_listener_service);
        alertDialogBuilder.setMessage(R.string.notification_listener_service_explanation);
        alertDialogBuilder.setPositiveButton(R.string.yes,
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        startActivity(new Intent(ACTION_NOTIFICATION_LISTENER_SETTINGS));
                    }
                });
        alertDialogBuilder.setNegativeButton(R.string.no,
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        // If you choose to not enable the notification listener
                        // the app. will not work as expected
                    }
                });
        return(alertDialogBuilder.create());
    }
}
