package com.superpowered.superpoweredlatency;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.pm.PackageManager;
import android.preference.PreferenceManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.ActionBarActivity;
import android.os.Bundle;
import android.util.AndroidRuntimeException;
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

import com.samsung.android.sdk.SsdkUnsupportedException;
import com.samsung.android.sdk.professionalaudio.Sapa;
import com.samsung.android.sdk.professionalaudio.SapaProcessor;
import com.samsung.android.sdk.professionalaudio.SapaService;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class MainActivity extends ActionBarActivity {
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
    private SapaService sapaService = null;
    private SapaProcessor sapaClient = null;
    private boolean bufferSizeOverride = false;
    private boolean headphoneSocket = false;
    private boolean proAudioFlag = false;
    private boolean sapaPermissions = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT >= 23) { // Need to ask for permissions.
            String[] permissions = {
                    Manifest.permission.RECORD_AUDIO,
                    Manifest.permission.MODIFY_AUDIO_SETTINGS,
                    Manifest.permission.INTERNET,
                    "com.samsung.android.sdk.professionalaudio.permission.START_MONITOR_SERVICE",
                    "com.samsung.android.providers.context.permission.WRITE_USE_APP_FEATURE_SURVEY"
            };
            for (String s:permissions) if (ContextCompat.checkSelfPermission(this, s) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, permissions, 0);
                break;
            }
        } else initialize();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
        if ((requestCode != 0) || (grantResults.length < 1) || (grantResults.length != permissions.length)) return;
        boolean canInitialize = true;
        for (int n = 0; n < grantResults.length; n++) if (grantResults[n] != PackageManager.PERMISSION_GRANTED) {
            if (n < 3) { // Record audio, modify audio settings and internet are showstoppers.
                canInitialize = false;
                Toast.makeText(getApplicationContext(), "Please allow all permissions for the Latency Test app.", Toast.LENGTH_LONG).show();
            } else sapaPermissions = false; // There is no Samsung Professional Audio SDK available in this case.
        }
        if (canInitialize) initialize();
    }

    private void initialize() {
        setContentView(R.layout.activity_main);

        // Set up UI and display system information.
        try {
            TextView title = (TextView)findViewById(R.id.header);
            title.setText("Superpowered Latency Test v" + getPackageManager().getPackageInfo(getPackageName(), 0).versionName);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }

        TextView model = (TextView)findViewById(R.id.model);
        model.setText(Build.MANUFACTURER + " " + Build.MODEL);
        TextView os = (TextView)findViewById(R.id.os);
        os.setText("Android " + Build.VERSION.RELEASE + " " + Build.ID);

        button = (TextView)findViewById(R.id.button);
        infoView = (TextView)findViewById(R.id.infoview);
        statusView = (RelativeLayout)findViewById(R.id.statusview);
        resultsView = (RelativeLayout)findViewById(R.id.resultsview);
        status = (TextView)findViewById(R.id.status);
        network = (TextView)findViewById(R.id.network);
        progress = (ProgressBar)findViewById(R.id.progress);
        website = (TextView)findViewById(R.id.website);

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

        // Use the Samsung Professional Audio SDK, if available.
        if ((Build.VERSION.SDK_INT >= 21) && sapaPermissions && PreferenceManager.getDefaultSharedPreferences(getBaseContext()).getBoolean("sapa", true)) {
            try {
                Sapa sapa = new Sapa();
                sapa.initialize(this);
                sapaService = new SapaService();
                sapaService.stop(true);
                sapaService.start(SapaService.START_PARAM_LOW_LATENCY);
                sapaClient = new SapaProcessor(this, null, new SapaProcessor.StatusListener() {
                    @Override
                    public void onKilled() {
                        sapaService.stop(true);
                        finish();
                    }
                });
                sapaService.register(sapaClient);
            } catch (SsdkUnsupportedException | InstantiationException | AndroidRuntimeException e) {
                sapaService = null;
                sapaClient = null;
            }
        }

        // OpenSL ES with fast mixer, if there is no SAPA.
        if (sapaClient == null) {
            System.loadLibrary("SuperpoweredLatency");
            // Get the device's sample rate and buffer size to enable low-latency Android audio output, if available.
            String samplerateString = null, buffersizeString = null;
            if (Build.VERSION.SDK_INT >= 17) {
                AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
                samplerateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
                buffersizeString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            }
            if (samplerateString == null) samplerateString = "44100";
            if (buffersizeString == null) buffersizeString = "512";
            else if (Build.VERSION.SDK_INT >= 19) buffersizeString = "-" + buffersizeString; // Indicate Android 4.4 or higher with negating the buffer size.

            // Override the buffer size if needed.
            String manualBuffersize = PreferenceManager.getDefaultSharedPreferences(getBaseContext()).getString("manual_buffersize", "0");
            int manualBuffersizeInt = Integer.parseInt(manualBuffersize);
            if ((manualBuffersizeInt >= 32) && (manualBuffersizeInt <= 2048)) {
                buffersizeString = manualBuffersize;
                bufferSizeOverride = true;
            };

            // Call the native lib to set up.
            SuperpoweredLatency(Integer.parseInt(samplerateString), Integer.parseInt(buffersizeString));
        } else {
            startService(new Intent(this, SapaStopService.class));
            sapaClient.activate();
        }

        // Update UI every 40 ms.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                UI_update();
                handler.postDelayed(this, 40);
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
    public void UI_update() {
        // Get the current state and latency values.
        long _state, _latencyMs;
        if (sapaService == null) {
            _state = getState();
            _latencyMs = getLatencyMs();
        } else {
            ByteBuffer buffer = sapaClient.queryData("s", 0);
            if (buffer == null) return;
            _state = buffer.get(0);
            buffer = sapaClient.queryData("l", 0);
            if (buffer == null) return;
            _latencyMs = buffer.get(0);
        }

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
                } else {
                    infoView.setVisibility(View.INVISIBLE);
                    resultsView.setVisibility(View.VISIBLE);
                    statusView.setVisibility(View.INVISIBLE);
                    website.setVisibility(View.INVISIBLE);

                    long samplerate, buffersize;
                    if (sapaService == null) {
                        samplerate = getSamplerate();
                        buffersize = getBuffersize();
                    } else {
                        ByteBuffer buffer = sapaClient.queryData("r", 0);
                        if (buffer == null) return;
                        buffer.order(ByteOrder.LITTLE_ENDIAN);
                        samplerate = buffer.getInt();
                        buffer = sapaClient.queryData("b", 0);
                        if (buffer == null) return;
                        buffer.order(ByteOrder.LITTLE_ENDIAN);
                        buffersize = buffer.getInt();
                    }

                    ((TextView)findViewById(R.id.latency)).setText(_latencyMs + " ms");
                    ((TextView)findViewById(R.id.buffersize)).setText(Long.toString(buffersize));
                    ((TextView)findViewById(R.id.samplerate)).setText(samplerate + " Hz");
                    button.setText("Share Results");

                    if (!bufferSizeOverride && PreferenceManager.getDefaultSharedPreferences(getBaseContext()).getBoolean("submit", true)) {
                        // Uploading the result to our server. Results with native buffer sizes are reported only.
                        network.setText("Uploading data...");
                        String url = Uri.parse("http://superpowered.com/latencydata/input.php?ms=" + _latencyMs + "&samplerate=" + samplerate + "&buffersize=" + buffersize + "&sapa=" + ((sapaService == null) ? 0 : 1) + "&headphone=" + (headphoneSocket ? 1 : 0) + "&proaudio=" + (proAudioFlag ? 1 : 0))
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
            long state;
            if (sapaService == null) {
                toggleMeasurer();
                state = getState();
            } else {
                sapaClient.sendCommand("t");
                ByteBuffer buffer = sapaClient.queryData("s", 0);
                if (buffer == null) return;
                state = buffer.get(0);
            }
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
    private native void toggleMeasurer();
    private native int getState();
    private native int getLatencyMs();
    private native int getSamplerate();
    private native int getBuffersize();

    // This performs the data upload to our server.
    private class HTTPGetTask extends AsyncTask<String, Void, Boolean> {
        protected Boolean doInBackground(String... param) {
            boolean okay = false;
            try {
                URL url = new URL(param[0]);
                HttpURLConnection connection = (HttpURLConnection)url.openConnection();
                connection.setRequestMethod("GET");
                connection.setRequestProperty("Cache-Control", "no-cache");
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
