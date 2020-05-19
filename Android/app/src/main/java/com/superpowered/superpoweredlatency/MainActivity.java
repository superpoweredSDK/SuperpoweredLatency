package com.superpowered.superpoweredlatency;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.pm.PackageManager;
import android.preference.PreferenceManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.support.annotation.NonNull;
import android.util.Base64;
import android.view.Menu;
import android.view.MenuItem;
import android.media.AudioManager;
import android.os.Build;
import android.content.Context;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.os.Handler;
import android.net.Uri;
import android.os.AsyncTask;

import java.net.HttpURLConnection;
import java.net.URL;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import android.content.Intent;
import android.content.IntentFilter;

public class MainActivity extends AppCompatActivity {
    TextView button = null;
    TextView infoView = null;
    TextView status = null;
    TextView network = null;
    TextView website = null;
    RelativeLayout statusView = null;
    RelativeLayout resultsView = null;
    ProgressBar progress = null;
    int measurerState = -1000;
    private Handler handler;
    private boolean headphoneSocket = false;
    private boolean proAudioFlag = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean didAskForPermissions = false;

        if (Build.VERSION.SDK_INT >= 23) { // Need to ask for permissions.
            String[] permissions = {
                    Manifest.permission.RECORD_AUDIO,
                    Manifest.permission.MODIFY_AUDIO_SETTINGS,
                    Manifest.permission.INTERNET,
            };

            for (String s:permissions) if (ContextCompat.checkSelfPermission(this, s) != PackageManager.PERMISSION_GRANTED) {
                didAskForPermissions = true;
                ActivityCompat.requestPermissions(this, permissions, 0);
                break;
            }
        }

        if (!didAskForPermissions) initialize();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String permissions[], @NonNull int[] grantResults) {
        if ((requestCode != 0) || (grantResults.length < 1) || (grantResults.length != permissions.length)) return;
        boolean canInitialize = true;
        for (int grantResult:grantResults) if (grantResult != PackageManager.PERMISSION_GRANTED) {
            canInitialize = false;
            Toast.makeText(getApplicationContext(), "Please allow all permissions for the Latency Test app.", Toast.LENGTH_LONG).show();
        }
        if (canInitialize) initialize();
    }

    private void initialize() {
        setContentView(R.layout.activity_main);

        // Set up UI and display system information.
        try {
            TextView title = findViewById(R.id.header);
            title.setText("Superpowered Latency Test v" + getPackageManager().getPackageInfo(getPackageName(), 0).versionName);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }

        TextView model = findViewById(R.id.model);
        model.setText(Build.MANUFACTURER + " " + Build.MODEL);
        TextView os = findViewById(R.id.os);
        os.setText("Android " + Build.VERSION.RELEASE + " " + Build.ID);

        button = findViewById(R.id.button);
        infoView = findViewById(R.id.infoview);
        statusView = findViewById(R.id.statusview);
        resultsView = findViewById(R.id.resultsview);
        status = findViewById(R.id.status);
        network = findViewById(R.id.network);
        progress = findViewById(R.id.progress);
        website = findViewById(R.id.website);

        if (Build.VERSION.SDK_INT >= 21) { // Check if there is something in the headphone socket.
            IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
            registerReceiver(new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (intent.getAction().equals(Intent.ACTION_HEADSET_PLUG)) {
                        int state = intent.getIntExtra("state", -1);
                        headphoneSocket = (state > 0);
                    }
                }
            }, filter);
        }

        if (Build.VERSION.SDK_INT >= 23) { // Check for the pro audio flag.
            proAudioFlag = getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_PRO);
        }

        // Two native libs are built from the same code, one with and one without AAudio.
        try {
            System.loadLibrary("WithAAudio");
        } catch (UnsatisfiedLinkError e) { // If AAudio is not present, use the other one without it.
            System.loadLibrary("WithoutAAudio");
        }

        // Get the device's sample rate and buffer size to enable low-latency Android audio output, if available.
        AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
        String samplerateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        String buffersizeString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        if (samplerateString == null) samplerateString = "48000";
        if (buffersizeString == null) buffersizeString = "960";

        // Call the native lib to set up.
        SuperpoweredLatency(Integer.parseInt(samplerateString), Integer.parseInt(buffersizeString));

        // Update UI every 40 ms until UI_update returns with false.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                if (UI_update()) handler.postDelayed(this, 40);
            }
        };
        handler = new Handler();
        handler.postDelayed(runnable, 40);
    }

    // Encodes safe strings for network transfer.
    public String encodeString(String str) {
        return Base64.encodeToString(str.getBytes(), Base64.DEFAULT);
    }

    // Called periodically to update the UI.
    public boolean UI_update() {
        // Get the current state and latency values.
        long _state = getState();
        long _latencyMs = getLatencyMs();

        // Update the UI if the measurer's state has been changed.
        if ((measurerState != _state) || (_latencyMs < 0)) {
            measurerState = (int)_state;

            // Idle state.
            if (measurerState == 0) {
                infoView.setVisibility(View.VISIBLE);
                resultsView.setVisibility(View.INVISIBLE);
                statusView.setVisibility(View.INVISIBLE);
                button.setText("Start Latency Test");

            // Result or error.
            } else if (measurerState > 10) {
                if (_latencyMs == 0) {
                    status.setText("Dispersion too big, please try again.");
                    button.setText("Restart Test");
                    toggleMeasurer(false); // Stopping the measurer.
                } else {
                    infoView.setVisibility(View.INVISIBLE);
                    resultsView.setVisibility(View.VISIBLE);
                    statusView.setVisibility(View.INVISIBLE);
                    website.setVisibility(View.INVISIBLE);

                    long samplerate = getSamplerate();
                    long buffersize = getBuffersize();
                    boolean aaudio = getAAudio();

                    ((TextView)findViewById(R.id.latency)).setText(_latencyMs + " ms");
                    ((TextView)findViewById(R.id.buffersize)).setText(Long.toString(buffersize));
                    ((TextView)findViewById(R.id.samplerate)).setText(samplerate + " Hz");
                    ((TextView)findViewById(R.id.api)).setText(aaudio ? "AAudio" : "OpenSL ES");
                    button.setText("Share Results");

                    if (PreferenceManager.getDefaultSharedPreferences(getBaseContext()).getBoolean("submit", true)) {
                        // Uploading the result to our server. Results with native buffer sizes are reported only.
                        network.setText("Uploading data...");
                        String url = Uri.parse("https://superpowered.com/latencydata/input.php?ms=" + _latencyMs + "&samplerate=" + samplerate + "&buffersize=" + buffersize + "&sapa=0&headphone=" + (headphoneSocket ? 1 : 0) + "&proaudio=" + (proAudioFlag ? 1 : 0) + "&aaudio=" + (aaudio ? 1 : 0))
                                .buildUpon()
                                .appendQueryParameter("model", encodeString(Build.MANUFACTURER + " " + Build.MODEL))
                                .appendQueryParameter("os", encodeString(Build.VERSION.RELEASE))
                                .appendQueryParameter("build", encodeString(Build.VERSION.INCREMENTAL))
                                .appendQueryParameter("buildid", encodeString(Build.ID))
                                .build().toString();
                        new HTTPGetTask().execute(url);
                    } else {
                        network.setText("");
                        website.setVisibility(View.VISIBLE);
                    }

                    toggleMeasurer(false); // Stopping the measurer.
                    return false; // Stopping periodical UI updates.
                }

            // Measurement starts.
            } else if (measurerState == 1) {
                statusView.setVisibility(View.VISIBLE);
                resultsView.setVisibility(View.INVISIBLE);
                infoView.setVisibility(View.INVISIBLE);
                status.setText("? ms");
                progress.setProgress(0);
                button.setText("Cancel");

            // Measurement in progress.
            } else {
                if (_latencyMs < 0) status.setText("The environment is too loud!");
                else {
                    status.setText(_latencyMs + " ms");
                    progress.setProgress((measurerState - 1) * 10);
                }
            }
        }
        return true;
    }

    // When the user taps on the main button.
    public void onButton(View _button) {
        if (resultsView.getVisibility() == View.VISIBLE) { // Sharing results - let's hope this creates a small viral loop for us. :-)
            String model = ((TextView)findViewById(R.id.model)).getText().toString();
            if (model.length() > 40) model = model.substring(0, 40);
            String message = "I just tested my " + model + " with the Superpowered Latency Test App: " + ((TextView)findViewById(R.id.latency)).getText() + "\r\n\r\nhttp://superpowered.com/latency";
            Intent share = new Intent(Intent.ACTION_SEND);
            share.setType("text/plain");
            share.putExtra(Intent.EXTRA_TEXT, message);
            startActivity(Intent.createChooser(share, "Share Results"));
        } else {
            toggleMeasurer(PreferenceManager.getDefaultSharedPreferences(getBaseContext()).getBoolean("aaudio", false));
            long state = getState();
            button.setText(state > 0 ? "Cancel" : "Start Latency Test");
        }
    }

    public void onLatency(View _view) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://superpowered.com/latency"));
        startActivity(browserIntent);
    }

    public void onSuperpowered(View _view) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://superpowered.com/"));
        startActivity(browserIntent);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return super.onOptionsItemSelected(item);
    }

    public void onSettings(View _view) {
        Intent settingsIntent = new Intent(this, SettingsActivity.class);
        startActivity(settingsIntent);
    }

    private native void SuperpoweredLatency(long samplerate, long buffersize);
    private native void toggleMeasurer(boolean useAAudioIfAvailable);
    private native int getState();
    private native int getLatencyMs();
    private native int getSamplerate();
    private native int getBuffersize();
    private native boolean getAAudio();

    // This performs the data upload to our server.
    private class HTTPGetTask extends AsyncTask<String, Void, Boolean> {
        protected Boolean doInBackground(String... param) {
            boolean okay = false;
            try {
                URL url = new URL(param[0]);
                HttpURLConnection connection = (HttpURLConnection)url.openConnection();
                connection.setRequestMethod("GET");
                connection.setRequestProperty("Cache-Control", "no-cache");
                connection.setConnectTimeout(30000);
                connection.setReadTimeout(60000);
                try {
                    InputStream in = connection.getInputStream();
                    InputStreamReader reader = new InputStreamReader(in);
                    for (int bytesRead = 0; bytesRead < 5; bytesRead++) if (reader.read() < 0) {
                        if (bytesRead == 2) okay = true;
                        break;
                    }
                } catch(Exception e) {
                    e.printStackTrace();
                } finally {
                    connection.disconnect();
                }
            } catch (IOException e) {
                e.printStackTrace();
                return false;
            }
            return okay;
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (result) {
                network.setText("Thank you. We've uploaded the result to the latency benchmarking at:");
                website.setVisibility(View.VISIBLE);
            } else network.setText("Network error.");
        }
    }
}
