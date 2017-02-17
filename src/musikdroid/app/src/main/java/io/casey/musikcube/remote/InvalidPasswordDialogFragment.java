package io.casey.musikcube.remote;

import android.app.Dialog;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;

public class InvalidPasswordDialogFragment extends DialogFragment {
    public static final String TAG = "InvalidPasswordDialogFragment";

    public static InvalidPasswordDialogFragment newInstance() {
        return new InvalidPasswordDialogFragment();
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity())
            .setTitle(R.string.invalid_password_dialog_title)
            .setMessage(R.string.invalid_password_dialog_message)
            .setPositiveButton(R.string.button_settings, (dialog, which) -> {
                startActivity(SettingsActivity.getStartIntent(getActivity()));
            })
            .setNegativeButton(R.string.button_close, null)
            .create();
    }
}
