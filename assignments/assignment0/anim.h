#include <vector>
#include <ew/transform.h>

enum KeyType {
	POS = 0,
	ROT,
	SCA,

	NUM_TYPES
};

struct Keyframe3 {
	float time = 0.0f;
	float values[3] = {0.0f, 0.0f, 0.0f};
};

class AnimationClip {
public:
	float duration = 0.0f;
	std::vector<Keyframe3> posKeys;
	std::vector<Keyframe3> rotKeys;
	std::vector<Keyframe3> scaKeys;

	// returns the address of the added keyframe, or NULL if no keyframe was added
	Keyframe3* AddKeyframe(KeyType type) {
		switch(type) {
		case POS:
			posKeys.push_back(Keyframe3());
			return &posKeys.back();
		case ROT:
			rotKeys.push_back(Keyframe3());
			return &rotKeys.back();
		case SCA:
			scaKeys.push_back(Keyframe3());
			return &scaKeys.back();
		default: // invalid type
			return NULL;
		}
	}

	// returns true if a keyframe was removed, false if not
	bool RemoveLastKeyframe(KeyType type) {
		switch(type) {
		case POS:
			if(posKeys.empty()) { return false; }
			posKeys.pop_back();
			return true;
		case ROT:
			if(rotKeys.empty()) { return false; }
			rotKeys.pop_back();
			return true;
		case SCA:
			if(scaKeys.empty()) { return false; }
			scaKeys.pop_back();
			return true;
		default: // invalid type
			return false;
		}
	}

	void EnsureAscendingTimes() {
		for(int i = 1; i < posKeys.size(); i++) {
			if(posKeys[i - 1].time > posKeys[i].time) { posKeys[i].time = posKeys[i - 1].time; }
		}
		for(int i = 1; i < rotKeys.size(); i++) {
			if(rotKeys[i - 1].time > rotKeys[i].time) { rotKeys[i].time = rotKeys[i - 1].time; }
		}
		for(int i = 1; i < scaKeys.size(); i++) {
			if(scaKeys[i - 1].time > scaKeys[i].time) { scaKeys[i].time = scaKeys[i - 1].time; }
		}
	}
};

class Animator {
public:
	AnimationClip* clip = NULL;
	ew::Transform* target = NULL;
	bool isPlaying = false;
	bool isLooping = false;
	float playbackSpeed = 1.0f; // 1 == normal speed
	float playbackTime = 0.0f;

	void PlayClip() {
		if(clip == NULL) { return; }
		if(isPlaying) {


			for(int i = 0; i < clip->posKeys.size(); i++) {
				if(clip->posKeys[i].time >= playbackTime) {
					float betweenPercent = InvLerp(clip->posKeys[i - 1].time, clip->posKeys[i].time, playbackTime);
					std::vector<float> lerpedPos = Lerp3(clip->posKeys[i].values, clip->posKeys[i].values, betweenPercent);
					target->position.x = lerpedPos[0];
					target->position.y = lerpedPos[1];
					target->position.z = lerpedPos[2];
				}
			}
		}
	}

	// returns a vector with 3 elements
	// function will crash program if A and B do not have at least 3 elements (lol)
	std::vector<float> Lerp3(const float* const A, const float* const B, float t) {
		// clamp percentage
		if(t < 0) { t = 0.0f; }
		else if(t > 1) { t = 1.0f; }

		std::vector<float> output;
		output.push_back(((1.0f - t) * A[0]) + (t * B[0]));
		output.push_back(((1.0f - t) * A[1]) + (t * B[1]));
		output.push_back(((1.0f - t) * A[2]) + (t * B[2]));

		return output;
	}

	float InvLerp(float A, float B, float x) { return (x - A) / (B - A); }
};
