package io.casey.musikcube.remote.ui.home.fragment

import android.app.Dialog
import android.os.Bundle
import android.support.v4.app.DialogFragment
import android.support.v7.app.AlertDialog

import io.casey.musikcube.remote.R
import io.casey.musikcube.remote.ui.settings.activity.SettingsActivity

class InvalidPasswordDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val activity = this.activity!!
        return AlertDialog.Builder(activity)
            .setTitle(R.string.invalid_password_dialog_title)
            .setMessage(R.string.invalid_password_dialog_message)
            .setPositiveButton(R.string.button_settings) { _, _ ->
                startActivity(SettingsActivity.getStartIntent(activity))
            }
            .setNegativeButton(R.string.button_close, null)
            .create()
    }

    companion object {
        const val TAG = "InvalidPasswordDialogFragment"

        fun newInstance(): InvalidPasswordDialogFragment {
            return InvalidPasswordDialogFragment()
        }
    }
}
