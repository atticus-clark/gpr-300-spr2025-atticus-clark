using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Knot : MonoBehaviour {
    public GameObject childArm;
    public GameObject coveredEndpoint;
    public GameObject mirroredArm;

    private void Update() {
        RepositionDependents();
    }

    void RepositionDependents() {
        if(coveredEndpoint != null) { coveredEndpoint.transform.position = transform.position; }
        if(mirroredArm != null) {
            mirroredArm.transform.position = 2 * transform.position - childArm.transform.position;
        }
    }
}
