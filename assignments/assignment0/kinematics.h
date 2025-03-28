#pragma once
#include <math.h>
#include <ew/transform.h>

struct Joint {
	const char* name;
	unsigned int parentIndex;
};

struct JointPose {
	glm::vec3 scale;
	glm::vec3 rotation; // Euler angles in degrees
	glm::vec3 position;
};

struct Skeleton {
	unsigned int numJoints;
	Joint* a_joints;
	JointPose* a_localPoses;
	glm::mat4* a_globalPoses;
};

// code taken from Transform.modelMatrix() by Eric Winebrenner
// ..\core\ew\transform.h
glm::mat4 TransformMatrix(JointPose pose) {
	glm::mat4 matrix = glm::mat4(1.0f);
	matrix = glm::translate(matrix, pose.position); 
	matrix *= glm::mat4_cast(glm::quat(glm::radians(pose.rotation)));
	matrix = glm::scale(matrix, pose.scale);
	return matrix;
}

// method for extracting translation, scale, and rotation from matrix from StackOverflow answer
// top answer (used here) works for matrix with only translation, rotation, and nonnegative scaling
// https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati
JointPose UndoTransformMatrix(const glm::mat4& matrix) {
	glm::mat4 copyMat = matrix;
	JointPose pose;

	// translation
	pose.position = glm::vec3(copyMat[3]);
	copyMat[3] = glm::vec4(0, 0, 0, 1);

	// scale
	pose.scale.x = glm::length(glm::vec3(copyMat[0]));
	pose.scale.y = glm::length(glm::vec3(copyMat[1]));
	pose.scale.z = glm::length(glm::vec3(copyMat[2]));

	// rotation
	copyMat[0] = glm::vec4(copyMat[0][0] / pose.scale.x, copyMat[0][1] / pose.scale.y, copyMat[0][2] / pose.scale.z, 0.0);
	copyMat[1] = glm::vec4(copyMat[1][0] / pose.scale.x, copyMat[1][1] / pose.scale.y, copyMat[1][2] / pose.scale.z, 0.0);
	copyMat[2] = glm::vec4(copyMat[2][0] / pose.scale.x, copyMat[2][1] / pose.scale.y, copyMat[2][2] / pose.scale.z, 0.0);
	pose.rotation = glm::degrees(glm::eulerAngles(glm::quat(copyMat)));

	return pose;
}

void SolveFK(const Skeleton& hierarchy) {
	// 0 is always the root bc child joints are required to come after their parent
	hierarchy.a_globalPoses[0] = TransformMatrix(hierarchy.a_localPoses[0]);

	for(int i = 1; i < hierarchy.numJoints; i++) {
		hierarchy.a_globalPoses[i] = hierarchy.a_globalPoses[hierarchy.a_joints[i].parentIndex]
			* TransformMatrix(hierarchy.a_localPoses[i]);
	}
}
