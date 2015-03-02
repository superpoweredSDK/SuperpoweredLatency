package com.superpowered.superpoweredlatency;

import android.app.Service;
import android.os.IBinder;
import android.content.Intent;
import android.util.AndroidRuntimeException;

import com.samsung.android.sdk.professionalaudio.SapaService;

// SAPA does not close automatically when the main activity is terminated, and the main activity's onDestroy method is not called every time.
// This service reliably detects when the main activity terminates, and stops SAPA (which also stops WAIOService).
// This service is not needed for OpenSL ES, as it closes the native process automatically when the main activity terminates.
public class SapaStopService extends Service {
    @Override
    public IBinder onBind(Intent arg0) {
        return null;
    }

    public void onTaskRemoved (Intent rootIntent) {
        try {
            SapaService sapaService = new SapaService(); // This is just a "connection" to the SAPA daemon.
            sapaService.stop(true); // Stop all SAPA stuff on this Android device.
        } catch (InstantiationException e) {
        } catch (AndroidRuntimeException e){
        }
        this.stopSelf();
    }
}