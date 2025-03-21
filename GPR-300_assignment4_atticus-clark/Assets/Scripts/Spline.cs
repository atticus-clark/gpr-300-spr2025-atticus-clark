using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Spline : MonoBehaviour {
    [SerializeField] GameObject curvePrefab;
    [SerializeField] GameObject endpointPrefab;
    [SerializeField] GameObject knotPrefab;
    [SerializeField] GameObject controlArmPrefab;

    [HideInInspector] public List<Curve> curves = new List<Curve>();

    public void CreateCurve() {
        int numCurves = curves.Count;
        Curve newCurve = Instantiate(curvePrefab).GetComponent<Curve>();
        Knot p0Knot = null;

        if(numCurves < 1) { newCurve.P0 = Instantiate(endpointPrefab); } // new curve needs 2 endpoints
        else { // this curve's P0 == prev curve's P3
            newCurve.P0 = Instantiate(knotPrefab);
            newCurve.P0.transform.position = curves[numCurves - 1].P3.transform.position;

            // cover prev curve's endpoint with know, grab mirrored arm
            p0Knot = newCurve.P0.GetComponent<Knot>();
            p0Knot.coveredEndpoint = curves[numCurves - 1].P3;
            p0Knot.mirroredArm = curves[numCurves - 1].P2;
        }

        newCurve.P3 = Instantiate(endpointPrefab);
        newCurve.P2 = Instantiate(controlArmPrefab, newCurve.P3.transform);
        newCurve.P1 = Instantiate(controlArmPrefab, newCurve.P0.transform);
        if(p0Knot != null) { p0Knot.childArm = newCurve.P1; }

        newCurve.DefaultCurve();
        curves.Add(newCurve);
        return;
    }

    public void DeleteLastCurve() {
        int destroyCurveIndex = curves.Count - 1;
        if(destroyCurveIndex < 0) { return; } // no curves to delete

        Curve destroyCurve = curves[destroyCurveIndex];
        Destroy(destroyCurve.P0);
        Destroy(destroyCurve.P1);
        Destroy(destroyCurve.P2);
        Destroy(destroyCurve.P3);

        Destroy(destroyCurve.gameObject);
        curves.RemoveAt(destroyCurveIndex);
    }
}
