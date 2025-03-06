#include <vector>
#include <ew/external/glad.h>
#include <ew/transform.h>

static class Util {
public:
	static glm::vec3 Lerp3(const glm::vec3 A, const glm::vec3 B, float t) {
		// clamp percentage
		if(t < 0.0f) { t = 0.0f; }
		else if(t > 1.0f) { t = 1.0f; }

		glm::vec3 output;
		output.x = A.x + (B.x - A.x) * t;
		output.y = A.y + (B.y - A.y) * t;
		output.z = A.z + (B.z - A.z) * t;

		return output;
	}

	static float InvLerp(const float A, const float B, const float x) { return (x - A) / (B - A); }
};

enum KeyType {
	POS = 0,
	ROT,
	SCA,

	NUM_TYPES
};

struct Keyframe3 {
	float time = 0.0f;
	glm::vec3 values;
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
			scaKeys.back().values = glm::vec3(1.0f, 1.0f, 1.0f);
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

		return;
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

	void PlayClip(float dt) {
		if(clip == NULL || target == NULL || !isPlaying || playbackSpeed == 0) { return; }
		playbackTime += dt * playbackSpeed;

		// if at end/beginning of clip (normal/reverse playback), loop or early exit
		if(playbackSpeed < 0) {
			if(playbackTime < 0) {
				if(isLooping) { playbackTime = clip->duration; }
				else {
					playbackTime = 0;
					return;
				}
			}
		}
		else {
			if(playbackTime > clip->duration) {
				if(isLooping) { playbackTime = 0; }
				else {
					playbackTime = clip->duration;
					return;
				}
			}
		}

		// scale
		for(int i = 0; i < clip->scaKeys.size(); i++) { // 0 keyframes case is skipped
			if(clip->scaKeys[i].time >= playbackTime) {
				// first keyframe case
				if(i == 0) { target->scale = clip->scaKeys[0].values; }

				else {
					float lerpPercent = Util::InvLerp(clip->scaKeys[i - 1].time, clip->scaKeys[i].time, playbackTime);
					target->scale = Util::Lerp3(clip->scaKeys[i - 1].values, clip->scaKeys[i].values, lerpPercent);
				}

				i = clip->scaKeys.size();
			}
		}

		// rotation
		for(int i = 0; i < clip->rotKeys.size(); i++) { // 0 keyframes case is skipped
			if(clip->rotKeys[i].time >= playbackTime) {
				// first keyframe case
				if(i == 0) { target->rotation = glm::quat(glm::radians(clip->rotKeys[0].values)); }

				else {
					float lerpPercent = Util::InvLerp(clip->rotKeys[i - 1].time, clip->rotKeys[i].time, playbackTime);

					glm::quat rot1AsQuat(glm::radians(clip->rotKeys[i - 1].values));
					glm::quat rot2AsQuat(glm::radians(clip->rotKeys[i].values));

					target->rotation = glm::slerp(rot1AsQuat, rot2AsQuat, lerpPercent);
				}

				i = clip->rotKeys.size();
			}
		}

		// position
		for(int i = 0; i < clip->posKeys.size(); i++) { // 0 keyframes case is skipped
			if(clip->posKeys[i].time >= playbackTime) {
				// first keyframe case
				if(i == 0) { target->position = clip->posKeys[0].values; }

				else {
					float lerpPercent = Util::InvLerp(clip->posKeys[i - 1].time, clip->posKeys[i].time, playbackTime);
					target->position = Util::Lerp3(clip->posKeys[i - 1].values, clip->posKeys[i].values, lerpPercent);
				}

				i = clip->posKeys.size();
			}
		}

		return;
	}
};
