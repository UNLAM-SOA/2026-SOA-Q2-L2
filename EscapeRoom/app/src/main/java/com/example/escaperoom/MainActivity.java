package com.example.escaperoom;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

public class MainActivity extends AppCompatActivity {

    private static final int[] CARD_IDS = {
            R.id.challenge1, R.id.challenge2, R.id.challenge3};
    private static final int[] TITLES = {
            R.string.challenge_1_title, R.string.challenge_2_title, R.string.challenge_3_title};
    private static final int[] DESCS = {
            R.string.challenge_1_desc, R.string.challenge_2_desc, R.string.challenge_3_desc};
    private static final int[] COLORS = {
            R.color.challenge_1, R.color.challenge_2, R.color.challenge_3};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        for (int i = 0; i < CARD_IDS.length; i++) {
            final int challenge = i + 1;
            View card = findViewById(CARD_IDS[i]);

            ((TextView) card.findViewById(R.id.challengeTitle)).setText(TITLES[i]);
            ((TextView) card.findViewById(R.id.challengeDesc)).setText(DESCS[i]);
            ((ImageView) card.findViewById(R.id.challengeIcon))
                    .setColorFilter(ContextCompat.getColor(this, COLORS[i]));

            card.setOnClickListener(v -> openChallenge(challenge));
        }
    }

    private void openChallenge(int challenge) {
        Intent intent = new Intent(this, ChallengeActivity.class);
        intent.putExtra(ChallengeActivity.EXTRA_CHALLENGE, challenge);
        startActivity(intent);
    }
}
