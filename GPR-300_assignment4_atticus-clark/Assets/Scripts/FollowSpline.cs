using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class FollowSpline : MonoBehaviour {
    [SerializeField] Spline spline;
    [SerializeField] float timeSpeed = 1.0f;

    double elapsedTime = 0.0;

    private void Update() {
        if(spline.curves.Count > 0) {
            elapsedTime += Time.deltaTime * timeSpeed;
            if(elapsedTime > spline.curves.Count) { elapsedTime = 0; } // looping
            float t = (float)(elapsedTime - (int)elapsedTime);
            Curve currentCurve = spline.curves[(int)elapsedTime];

            UpdatePosition(t, currentCurve);
            UpdateRotation(t, currentCurve);
        }
    }

    void UpdatePosition(float t, Curve curve) {
        transform.position = BezierCubic.Bernstein(t,
            curve.P0.transform.position,
            curve.P1.transform.position,
            curve.P2.transform.position,
            curve.P3.transform.position);
    }

    void UpdateRotation(float t, Curve curve) {
        Vector3 tangent = BezierCubic.BernsteinD1(t,
            curve.P0.transform.position,
            curve.P1.transform.position,
            curve.P2.transform.position,
            curve.P3.transform.position);

        transform.LookAt(transform.position + tangent);
    }
}
