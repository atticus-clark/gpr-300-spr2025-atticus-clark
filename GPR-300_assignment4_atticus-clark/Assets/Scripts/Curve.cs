using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Curve : MonoBehaviour {
    public GameObject P0;
    public GameObject P1;
    public GameObject P2;
    public GameObject P3;

    List<Vector3> drawPoints = new List<Vector3>();
    [SerializeField] Color curveColor = Color.white;
    public int numDrawSegments = 30;

    private void Update() {
        RedrawCurve(numDrawSegments);
    }

    private void OnDrawGizmos() {
        // draw the curve by drawing line segments approximating it
        Gizmos.color = curveColor;
        for(int i = 1; i < drawPoints.Count; i++) {
            Gizmos.DrawLine(drawPoints[i - 1], drawPoints[i]);
        }
    }

    public void RedrawCurve(int numSegments) {
        drawPoints.Clear();

        for(int i = 0; i <= numSegments; i++) {
            drawPoints.Add(BezierCubic.Bernstein((float)i / numSegments, P0.transform.position,
                P1.transform.position, P2.transform.position, P3.transform.position));
        }

        // actual drawing is done in OnDrawGizmos, but would go here if it wasn't

        return;
    }

    // Sets control points to default positions based on the position of P0.
    public void DefaultCurve() {
        Vector3 p0Pos = P0.transform.position;
        P1.transform.position = new Vector3(p0Pos.x + 1, p0Pos.y, p0Pos.z + 1);
        P2.transform.position = new Vector3(p0Pos.x + 2, p0Pos.y, p0Pos.z + 1);
        P3.transform.position = new Vector3(p0Pos.x + 3, p0Pos.y, p0Pos.z);

        return;
    }
}
