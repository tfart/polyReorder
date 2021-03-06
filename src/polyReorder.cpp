/**
    Copyright (c) 2017 Ryan Porter    
    You may use, distribute, or modify this code under the terms of the MIT license.
*/

#include "polyReorder.h"

#include <vector>

#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MStatus.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>


#define RETURN_IF_ERROR(s) if (!s) { return s; }


MStatus polyReorder::getPoints(MObject &mesh, MIntArray &pointOrder, MPointArray &outPoints)
{
    MFnMesh meshFn(mesh);

    uint numVertices = meshFn.numVertices();

    MPointArray inPoints(numVertices);
    outPoints.setLength(numVertices);

    meshFn.getPoints(inPoints, MSpace::kObject);
    
    for (uint i = 0; i < numVertices; i++)
    {
        outPoints.set(inPoints[i], pointOrder[i]);
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::getPolys(MObject &mesh, MIntArray &pointOrder, MIntArray &polyCounts, MIntArray &polyConnects, bool reorderPoints)
{
    MFnMesh meshFn(mesh);
    meshFn.getVertices(polyCounts, polyConnects);

    if (reorderPoints)
    {
        for (uint i = 0; i < polyConnects.length(); i++)
        {
            polyConnects[i] = pointOrder[polyConnects[i]];
        }
    }

    return MStatus::kSuccess;
}


void polyReorder::getFaceVertexList(MIntArray &polyCounts, MIntArray &polyConnects, MIntArray &faceList, MIntArray &vertexList)
{
    uint numPolys = polyCounts.length();
    uint numNormals = polyConnects.length();

    faceList.setLength(numNormals);
    vertexList.setLength(numNormals);

    uint idx = 0;

    for (uint i = 0; i < numPolys; i++)
    {
        for (int j = 0; j < polyCounts[i]; j++, idx++)
        {
            faceList[idx] = i;
            vertexList[idx] = polyConnects[idx];
        }
    }
}


MStatus polyReorder::getFaceVertexNormals(MObject &mesh, MVectorArray &vertexNormals)
{
    MStatus status;

    uint i = 0;

    for (MItMeshPolygon itPoly(mesh); !itPoly.isDone(); itPoly.next())
    {
        uint numVertices = itPoly.polygonVertexCount();

        for (uint v = 0; v < numVertices; v++, i++)
        {
            itPoly.getNormal(v, vertexNormals[i], MSpace::kObject);
        }
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::setFaceVertexNormals(MObject &mesh, MIntArray &polyCounts, MIntArray &polyConnects, MVectorArray &vertexNormals)
{
    MStatus status;

    MFnMesh meshFn(mesh);

    MIntArray faceList;
    MIntArray vertexList;

    polyReorder::getFaceVertexList(polyCounts, polyConnects, faceList, vertexList);
    
    status = meshFn.setFaceVertexNormals(vertexNormals, faceList, vertexList, MSpace::kObject);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = meshFn.unlockFaceVertexNormals(faceList, vertexList);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MStatus::kSuccess;
}

MStatus polyReorder::getFaceVertexLocks(MObject &mesh, MIntArray &lockedList)
{
    MStatus status;

    MFnMesh meshFn(mesh);

    MIntArray normalCounts;
    MIntArray normalIds;

    status = meshFn.getNormalIds(normalCounts, normalIds);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    uint numNormals = normalIds.length();

    lockedList.setLength(numNormals);

    for (uint i = 0; i < numNormals; i++)
    {
        lockedList[i] = meshFn.isNormalLocked(normalIds[i]);
    }

    return MStatus::kSuccess;
}

MStatus polyReorder::setFaceVertexLocks(MObject &mesh, MIntArray &lockedList)
{
    MStatus status;

    MFnMesh meshFn(mesh);

    MIntArray faceList;
    MIntArray vertexList;

    MIntArray polyCounts;
    MIntArray polyConnects;

    status = meshFn.getVertices(polyCounts, polyConnects);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    polyReorder::getFaceVertexList(polyCounts, polyConnects, faceList, vertexList);

    uint numNormals = polyConnects.length();

    MIntArray lockedFaceList(numNormals);
    MIntArray lockedVertList(numNormals);

    MIntArray unlockedFaceList(numNormals);
    MIntArray unlockedVertList(numNormals);

    uint L = 0;
    uint UL = 0;

    for (uint i = 0; i < numNormals; i++)
    {
        if (lockedList[i] == 1)
        {
            lockedFaceList[L] = faceList[i];
            lockedVertList[L] = vertexList[i];
            L++;
        } else {
            unlockedFaceList[UL] = faceList[i];
            unlockedVertList[UL] = vertexList[i];
            UL++;
        }
    }

    if (UL)
    {
        unlockedFaceList.setLength(UL);
        unlockedVertList.setLength(UL);
        status = meshFn.unlockFaceVertexNormals(unlockedFaceList, unlockedVertList);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (L)
    {
        lockedFaceList.setLength(L);
        lockedVertList.setLength(L);
        status = meshFn.lockFaceVertexNormals(lockedFaceList, lockedVertList);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::getEdgeSmoothing(MObject &mesh, MIntArray &pointOrder, std::unordered_map<uint64_t, bool> &edgeSmoothing)
{
    MStatus status;

    MItMeshEdge itEdge(mesh);

    while (!itEdge.isDone())
    {
        int v0 = pointOrder[itEdge.index(0)];
        int v1 = pointOrder[itEdge.index(1)];

        uint64_t edgeKey = polyReorder::twoIntKey(v0, v1);

        edgeSmoothing.emplace(edgeKey, itEdge.isSmooth());

        itEdge.next();
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::setEdgeSmoothing(MObject &mesh, std::unordered_map<uint64_t, bool> &edgeSmoothing)
{
    MStatus status;

    MItMeshEdge itEdge(mesh);

    while (!itEdge.isDone())
    {
        int v0 = itEdge.index(0);
        int v1 = itEdge.index(1);

        uint64_t edgeKey = polyReorder::twoIntKey(v0, v1);

        itEdge.setSmoothing(edgeSmoothing[edgeKey]);

        itEdge.next();
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::getUVs(MObject &mesh, std::vector<UVSetData> &uvSets)
{
    MStatus status;

    MFnMesh meshFn(mesh);

    int numUVSets = meshFn.numUVSets();

    MStringArray uvSetNames;
    uvSets.resize(numUVSets);
    meshFn.getUVSetNames(uvSetNames);
    
    for (int i = 0; i < numUVSets; i++)
    {
        uvSets[i].name = uvSetNames[i];
    }

    for (UVSetData &uvData : uvSets)
    {
        status = meshFn.getUVs(uvData.uArray, uvData.vArray, &(uvData.name));
        CHECK_MSTATUS_AND_RETURN_IT(status);

        status = meshFn.getAssignedUVs(uvData.uvCounts, uvData.uvIds, &(uvData.name));
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::setUVs(MObject &mesh, std::vector<UVSetData> &uvSets)
{
    MStatus status;

    MFnMesh meshFn(mesh);

    for (UVSetData &uvData : uvSets)
    {
        if (uvData.name != "map1")
        {
            status = meshFn.createUVSet(uvData.name);
            CHECK_MSTATUS(status);
        }

        status = meshFn.clearUVs(&(uvData.name));
        CHECK_MSTATUS_AND_RETURN_IT(status);

        status = meshFn.setUVs(uvData.uArray, uvData.vArray, &(uvData.name));
        CHECK_MSTATUS_AND_RETURN_IT(status);

        status = meshFn.assignUVs(uvData.uvCounts, uvData.uvIds, &(uvData.name));
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return MStatus::kSuccess;
}


MStatus polyReorder::reorderMesh(MObject &sourceMesh, MObject &targetMesh, MIntArray &pointOrder, MObject &outMesh, bool isMeshData)
{
    MStatus status;

    MFnMesh srcMeshFn(sourceMesh);
    MFnMesh tgtMeshFn(targetMesh);

    uint numVertices = srcMeshFn.numVertices();
    uint numPolys = srcMeshFn.numPolygons();

    MPointArray points;
    MIntArray polyCounts;
    MIntArray polyConnects;

    MIntArray faceList;
    MIntArray vertexList;
    MIntArray lockedList;

    MVectorArray vertexNormals;

    std::unordered_map<uint64_t, bool> edgeSmoothing;
    std::vector<UVSetData> uvSets;

    polyReorder::getPoints(targetMesh, pointOrder, points);
    polyReorder::getPolys(sourceMesh, pointOrder, polyCounts, polyConnects, isMeshData);

    vertexNormals.setLength(polyConnects.length());

    polyReorder::getFaceVertexNormals(targetMesh, vertexNormals);
    polyReorder::getFaceVertexLocks(targetMesh, lockedList);
    polyReorder::getEdgeSmoothing(targetMesh, pointOrder, edgeSmoothing);
    polyReorder::getUVs(targetMesh, uvSets);

    if (isMeshData)
    {
        MFnMesh outMeshFn;

        outMeshFn.create(
            numVertices, 
            numPolys,
            points,
            polyCounts,
            polyConnects,
            outMesh,
            &status
        );

        CHECK_MSTATUS_AND_RETURN_IT(status);
    } else {
        MFnMesh outMeshFn(outMesh);
        MFloatPointArray floatPoints(numVertices);

        for (uint i = 0; i < numVertices; i++)
        {
            floatPoints[i].setCast(points[i]);
        }

        outMeshFn.createInPlace(
            numVertices, 
            numPolys,
            floatPoints,
            polyCounts,
            polyConnects
        );

        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    status = polyReorder::setUVs(outMesh, uvSets);
    RETURN_IF_ERROR(status);

    status = polyReorder::setFaceVertexNormals(outMesh, polyCounts, polyConnects, vertexNormals);
    RETURN_IF_ERROR(status);

    status = polyReorder::setFaceVertexLocks(outMesh, lockedList);
    RETURN_IF_ERROR(status);

    status = polyReorder::setEdgeSmoothing(outMesh, edgeSmoothing);
    RETURN_IF_ERROR(status);

    return MStatus::kSuccess;
}