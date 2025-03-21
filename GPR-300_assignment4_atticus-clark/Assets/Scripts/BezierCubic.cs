using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public static class BezierCubic {
    // These functions are used to calculate a point on a cubic bezier curve, given
    // the curve's four control points and a lerp value t between 0 and 1 (inclusive).
    // Given the same inputs, all of these functions will return the same output.

    public static Vector3 DeCasteljau(float t, Vector3 P0, Vector3 P1, Vector3 P2, Vector3 P3) {
        Vector3 a = Vector3.Lerp(P0, P1, t);
        Vector3 b = Vector3.Lerp(P1, P2, t);
        Vector3 c = Vector3.Lerp(P2, P3, t);

        Vector3 d = Vector3.Lerp(a, b, t);
        Vector3 e = Vector3.Lerp(b, c, t);

        return Vector3.Lerp(d, e, t);
    }

    public static Vector3 Bernstein(float t, Vector3 P0, Vector3 P1, Vector3 P2, Vector3 P3) {
        float t2 = t * t, t3 = t * t * t;

        Vector3 a = P0 * ( -t3   +  3*t2 + -3*t + 1 );
        Vector3 b = P1 * (  3*t3 + -6*t2 +  3*t );
        Vector3 c = P2 * ( -3*t3 +  3*t2 );
        Vector3 d = P3 * (  t3 );

        return a + b + c + d;
    }

    // short for Polynomial Coefficients
    public static Vector3 PolyCoeffs(float t, Vector3 P0, Vector3 P1, Vector3 P2, Vector3 P3) {
        Vector3 a = 1     * (  P0 );
        Vector3 b = t     * ( -3*P0 +  3*P1 );
        Vector3 c = t*t   * (  3*P0 + -6*P1 +  3*P2 );
        Vector3 d = t*t*t * ( -P0   +  3*P1 + -3*P2 + P3 );

        return a + b + c + d;
    }

    // first derivatives
    public static Vector3 BernsteinD1(float t, Vector3 P0, Vector3 P1, Vector3 P2, Vector3 P3) {
        float t2 = t * t;

        Vector3 a = P0 * ( -3*t2 +   6*t - 3 );
        Vector3 b = P1 * (  9*t2 + -12*t + 3 );
        Vector3 c = P2 * ( -9*t2 +   6*t );
        Vector3 d = P3 * (  3*t2);

        return a + b + c + d;
    }
}
