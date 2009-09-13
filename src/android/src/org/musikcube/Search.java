package org.musikcube;

import android.app.TabActivity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TabHost;

public class Search extends TabActivity {
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.search);

        TabHost mTabHost = getTabHost();
        
        mTabHost.addTab(mTabHost.newTabSpec("tab_test1").setIndicator("Tracks").setContent(R.id.tracks));
        mTabHost.addTab(mTabHost.newTabSpec("tab_test2").setIndicator("Artists").setContent(R.id.artists));
        mTabHost.addTab(mTabHost.newTabSpec("tab_test3").setIndicator("Albums").setContent(R.id.albums));
        
        mTabHost.setCurrentTab(0);        
    }

}
