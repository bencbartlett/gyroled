#ifndef STATE_HPP
#define STATE_HPP

#include <Arduino.h>

// The synchronization packet sent over ESP‑NOW
struct State {

	bool isPaused = true;

    // Frame/tempo bookkeeping
    uint16_t frame            = 0;
    unsigned long time        = 0;
	unsigned long lastUpdate  = 0;
    float updatesPerSecond    = 50.0f; 

	unsigned long lastBeatTimestamp = 0;
	unsigned long elapsedBeats = 0;

    // Visual state
	uint8_t brightness    = 160; // 0-255
    uint8_t shader_index  = 0;
    float beat_intensity  = 0.0f;

    // Per‑ring telemetry
    float target_angle_1  = 0.0f;
	float target_angular_velocity_1 = 0.0f;

    float target_angle_2  = 0.0f;
	float target_angular_velocity_2 = 0.0f;

    float target_angle_3  = 0.0f;
	float target_angular_velocity_3 = 0.0f;

    float target_angle_4  = 0.0f;
	float target_angular_velocity_4 = 0.0f;

    float target_angle_5  = 0.0f;
	float target_angular_velocity_5 = 0.0f;

    float target_angle_6  = 0.0f;
	float target_angular_velocity_6 = 0.0f;

	void print() const {
		Serial.println(F("========== State =========="));
	
		// Top‑level flags & timing
		Serial.printf("Paused: %-5s   Frame: %-6u   Time: %9lu ms\n",
					  isPaused ? "true" : "false",
					  frame,
					  time);
		Serial.printf("LastUpd: %9lu ms   UPS: %5.1f\n",
					  lastUpdate,
					  updatesPerSecond);
		Serial.printf("BeatTs:  %9lu ms   Beats: %-6lu\n",
					  lastBeatTimestamp,
					  elapsedBeats);
	
		// Visual parameters
		Serial.printf("Brightness: %-3u   Shader: %-3u   BeatInt: %.2f\n",
					  brightness,
					  shader_index,
					  beat_intensity);
	
		// Per‑ring angles
		Serial.println(F("\nRing   Target°   ω (°/s)"));
		Serial.println(F("----   --------   --------"));
		Serial.printf("  1    %8.1f   %8.1f\n", target_angle_1, target_angular_velocity_1);
		Serial.printf("  2    %8.1f   %8.1f\n", target_angle_2, target_angular_velocity_2);
		Serial.printf("  3    %8.1f   %8.1f\n", target_angle_3, target_angular_velocity_3);
		Serial.printf("  4    %8.1f   %8.1f\n", target_angle_4, target_angular_velocity_4);
		Serial.printf("  5    %8.1f   %8.1f\n", target_angle_5, target_angular_velocity_5);
		Serial.printf("  6    %8.1f   %8.1f\n", target_angle_6, target_angular_velocity_6);	
		Serial.println(F("===========================\n"));
	}

};



#endif // STATE_HPP
