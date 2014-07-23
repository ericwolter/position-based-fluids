// OVERVIEW
//  The following stages are implemented in this tool:
//    1. Loading 3d object file
//    2. Finding surfaces and assigning each triangle to a surface
//        a. find all triangles area
//        b. sort traingles from largest to smallest
//        c. for all unassigned traingles (starting with the larger traingle):
//             i. find traingle surface equaction
//             ii. scan for other traingles that are placed on the surface
//             iii. if a match was found, remove paired traingle from the unassigned list
//    3. Expanding traingle edges
//        a. creating an edge list:
//           - each edge will have a normal that perpendicular to the edge and lays on the surface
//           - an edge that is shared between two triangles will have two normals
//           - the edge list is stored per surface, that means that if two traingles are connected 
//             but are placed in a diffrent surfaces, their edges will not consider connected.
//           - edges will only one normal are external edge (lays on the rim of the surface)
//           - edges with two normals are internal edges    (lays inside the surface)
//        b. create a vertex normal:
//           - normal lays on the surface (ie, if a vertex is shared between two surfaces, it will process several times seperatly).
//        c. compute how much so each vertex be moved along the normal to create a "fixed" distance between the old edge and the new edge.
//        d. move vertex to expanded position
//    4. Ajusting surface vectors (B, E0, E1) to:
//        - be perpendicular
//        - ensure that surface traingles are contained in s=0..1 and t=0..1
//        
//        a. Finding the best per-surface pivot point (a point that the surface base vector will point to
//           - scan all points of each surface finding the best point
//           - the "best point" is the one that have the two longest perpendicular (as much as possible) edges.
//        b. ensures that E0 and E1 are perpendicular
//        c. scale E0 and E1 to ensure that all traingles fit range of 0..1
//    5. Draw mask and debug images

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Sharp3D.Math.Core;

namespace ProcessForcePlanes
{

    class Program
    {
        static int ImgBorder = 10;

        class PQDResult
        {
            public float s;
            public float t;
            public float dist;
            public Vector3F quadPos;
            public bool Cull;
        }

        class EdgeInfo
        {
            public Vector3F p1;
            public Vector3F p2;
            public List<Vector3F> normals = new List<Vector3F>();
        }

        class SurfaceInfo
        {
            public List<Face> faces = new List<Face>();
            public Vector3F[] vertices;
            public Vector3F[] expanded_vertices;
            public List<EdgeInfo> edges = new List<EdgeInfo>();
            public Dictionary<Vector3F, Vector3F> vertexNormal = new Dictionary<Vector3F, Vector3F>();
            public Dictionary<Vector3F, List<EdgeInfo>> vertexEdges = new Dictionary<Vector3F, List<EdgeInfo>>();

            // Surface anchors (3 vertices that define the surface equaction)
            public Vector3F[] anchors;

            // Surface Equation
            public Vector3F surface_B, surface_E0, surface_E1;

            // Mask related
            public Size maskSize;
        }


        // http://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf
        static PQDResult PointQuadDistance(Vector3F P, Vector3F B, Vector3F E0, Vector3F E1)
        {
            var D = B - P;
            var a = Vector3F.DotProduct(E0, E0); // Precalc
            var b = Vector3F.DotProduct(E0, E1); // Precalc
            var c = Vector3F.DotProduct(E1, E1); // Precalc
            var d = Vector3F.DotProduct(E0, D);
            var e = Vector3F.DotProduct(E1, D);
            var f = Vector3F.DotProduct(D,  D);
            var invdet = 1 / (a * c - b * b);   // Precalc

            var res = new PQDResult();
            res.s = invdet * (b * e - c * d);
            res.t = invdet * (b * d - a * e);
            res.quadPos = B + E0 * res.s + E1 * res.t;
            res.Cull = Vector3F.DotProduct(Vector3F.CrossProduct(E0, E1), P - res.quadPos) > 0;
            res.dist = (P - res.quadPos).GetLength() * (res.Cull ? 1 : -1);

            return res;
        }

        static PointF PointInQuad2D(Vector3F P, Vector3F B, Vector3F E0, Vector3F E1, Rectangle rect)
        {
            var pointInQuad = PointQuadDistance(P, B, E0, E1);

            return new PointF(rect.Left + pointInQuad.t * rect.Width, rect.Top + pointInQuad.s * rect.Height);
        }


        static int[] FindFarthestPoints(Vector3F[] v)
        {
            var bestMacth_Dist = 0.0f;
            var bestMatch = new int[3] { -1, -1, -1 };
            for (int i1 = 0; i1 < v.Length; i1++)
                for (int i2 = i1 + 1; i2 < v.Length; i2++)
                    for (int i3 = i2 + 1; i3 < v.Length; i3++)
                    {
                        // get edges length
                        var dist1 = Vector3F.Subtract(v[i1], v[i2]).GetLengthSquared();
                        var dist2 = Vector3F.Subtract(v[i1], v[i3]).GetLengthSquared();
                        var dist3 = Vector3F.Subtract(v[i2], v[i3]).GetLengthSquared();
                        var dist = dist1 + dist2 + dist3;

                        // Check if pints are farther apart
                        if ((dist > bestMacth_Dist) && (dist1 > 0.0) && (dist2 > 0.0) && (dist3 > 0.0))
                        {
                            bestMacth_Dist = dist;
                            bestMatch[0] = i1;
                            bestMatch[1] = i2;
                            bestMatch[2] = i3;
                        }
                    }


            return bestMatch;
        }

        static int FindEdge(List<EdgeInfo> edgeList, Vector3F p1, Vector3F p2)
        {
            for (int i = 0; i < edgeList.Count; i++)
            {
                int pntCount = 0;

                if (Vector3F.Equals(edgeList[i].p1, p1)) pntCount++;
                if (Vector3F.Equals(edgeList[i].p2, p1)) pntCount++;
                if (Vector3F.Equals(edgeList[i].p1, p2)) pntCount++;
                if (Vector3F.Equals(edgeList[i].p2, p2)) pntCount++;

                if (pntCount == 2)
                    return i;
            }

            return -1;
        }

        static void UpdateEdgeList(List<EdgeInfo> edgeList, Vector3F p1, Vector3F p2, Vector3F faceNormal)
        {
            // Check if it's a new edge
            var edgeIndex = FindEdge(edgeList, p1, p2);
            if (edgeIndex == -1)
            {
                // Add new edge
                edgeIndex = edgeList.Count;
                edgeList.Add(new EdgeInfo() {p1 = p1, p2 = p2});
            }

            // Append normal
            var edgeNormal = Vector3F.CrossProduct(faceNormal, Vector3F.Subtract(p2, p1)).GetUnit();
            edgeList[edgeIndex].normals.Add(edgeNormal);
        }

        static string F2S(float value)
        {
            if (Math.Abs(value) < 0.00001)
                value = 0;

            var ret = value.ToString("G4");
            
            // Adding "+" sign
            //if (value >= 0)
                //ret = "+" + ret;

            return ret.PadLeft(8);
        }

        static void Main(string[] args)
        {
            // Check parameters
            if (args.Length == 0)
            {
                Console.WriteLine("Usage: ProcessForcePlanes <input.obj>");
                return;
            }

            // Get full object path
            var inputObj    = Path.GetFullPath(args[0]);
            var fileName    = Path.GetFileNameWithoutExtension(inputObj);
            var debugImages = Path.Combine(Path.GetDirectoryName(inputObj), @"DebugImages");
            var maskOutput  = Path.Combine(Path.GetDirectoryName(inputObj), @"..\textures\" + fileName + "_fp_mask.png");
            var equOutput   = Path.Combine(Path.GetDirectoryName(inputObj), @"..\kernels\" + fileName + "_equ.txt");
            Directory.CreateDirectory(debugImages);

            // Load object
            ObjLoader obj = new ObjLoader();
            obj.LoadObj(inputObj);

            string glslCode = "";
            var surfaceList = new List<SurfaceInfo>();
                    
            // Compute areas
            foreach (var face in obj.meshs.SelectMany(t => t.faces))
            {
                // Ensure tri
                if (face.positionIdx.Length != 3)
                    throw new Exception("Non-triangle polygon detected");

                // Compute quad area
                var v = face.vertices = face.positionIdx.Select(t => obj.positions[t-1]).ToArray();
                var E0 = Vector3F.Subtract(v[2], v[0]);
                var E1 = Vector3F.Subtract(v[1], v[0]);
                face.area = Vector3F.CrossProduct(E0, E1).GetLength();

                // Compute normal
                face.computedNormal = Vector3F.CrossProduct(E0, E1);
                if (face.computedNormal.GetLengthSquared() > 0)
                    face.computedNormal.Normalize();
                else
                    0.ToString();
                    
                // Tri has half of quad size
                face.area = face.area / 2;
            }

            // Remove zero area faces
            var faces = obj.meshs.SelectMany(t => t.faces).Where(t => t.area > 0).ToList();

            // Sort according to area
            faces.Sort((a, b) => b.area.CompareTo(a.area)); 

            // Scan triangles for faces
            for (int iFace = 0; iFace < faces.Count; iFace++)
            {
                // Check if face is marked for active force
                var face = faces[iFace];

                // Create a face list for it
                var surface = new SurfaceInfo();
                surfaceList.Add(surface);
                surface.faces.Add(face);

                // Get surface vectors
                var B = face.vertices[0];
                var E0 = Vector3F.Subtract(face.vertices[1], face.vertices[0]);
                var E1 = Vector3F.Subtract(face.vertices[2], face.vertices[0]);

                // Project all vertices agains quad
                for (int iOtherFaces = faces.Count - 1; iOtherFaces >= iFace + 1; iOtherFaces--)
                {
                    // Get "other" face
                    var othFace = faces[iOtherFaces];

                    // Ignore if diffrent normals
                    if (Vector3F.DotProduct(face.computedNormal, othFace.computedNormal) < 0.7)
                        continue;

                    // Ignore if any of the "other" triangle points is too far from surface
                    if (othFace.vertices.Any(vert => Math.Abs(PointQuadDistance(vert, B, E0, E1).dist) > 0.001))
                        continue;

                    // Add to this face triangle list
                    surface.faces.Add(othFace);

                    // Remove from mesh faces
                    faces.RemoveAt(iOtherFaces);
                }
            }

            // Sort according to size
            surfaceList.Sort((a, b) => a.faces.Count.CompareTo(b.faces.Count));

            // Build edge list
            foreach (var surface in surfaceList)
            {
                // Add all edges of all faces
                foreach (var face in surface.faces)
                {
                    UpdateEdgeList(surface.edges, face.vertices[0], face.vertices[1], face.computedNormal);
                    UpdateEdgeList(surface.edges, face.vertices[1], face.vertices[2], face.computedNormal);
                    UpdateEdgeList(surface.edges, face.vertices[2], face.vertices[0], face.computedNormal);
                }
            }

            // Expand edges
            foreach (var surface in surfaceList)
            {
                // Create surface vertices list
                surface.vertices = surface.faces.SelectMany(face => face.vertices).Distinct().ToArray();

                // Create vertex=>edges map
                foreach (var vertex in surface.vertices)
                    surface.vertexEdges[vertex] = surface.edges.Where(edge => (edge.p1 == vertex) || (edge.p2 == vertex)).ToList();

                foreach (var vertex in surface.vertices)
                {
                    foreach (var edge in surface.vertexEdges[vertex])
                    {
                        // Internal edge?
                        if (edge.normals.Count > 1)
                            continue;

                        // Create if doesn't exists
                        if (!surface.vertexNormal.ContainsKey(edge.p1)) surface.vertexNormal[edge.p1] = Vector3F.Zero;
                        if (!surface.vertexNormal.ContainsKey(edge.p2)) surface.vertexNormal[edge.p2] = Vector3F.Zero;

                        // Shift point
                        surface.vertexNormal[edge.p1] = Vector3F.Add(surface.vertexNormal[edge.p1], edge.normals[0]);
                        surface.vertexNormal[edge.p2] = Vector3F.Add(surface.vertexNormal[edge.p2], edge.normals[0]);
                    }
                }

                // Normalize and scale offset
                foreach (var vertex in surface.vertices)
                {
                    // Find one of the external edges of the vertex
                    var vertexExtEdge = surface.vertexEdges[vertex].FirstOrDefault(t => t.normals.Count == 1);

                    // Compute the ratio between the movement along the vertex normal and the movement along the edge normal
                    var vertexNormal = surface.vertexNormal[vertex].GetUnit();
                    var projectedNormalLen = Vector3F.DotProduct(vertexNormal, vertexExtEdge.normals[0]);

                    // Compute acutal offset
                    var vertexOffset = Vector3F.Multiply(vertexNormal, Math.Min(1.0f, 1.0f / projectedNormalLen));

                    // Move vertices
                    foreach (var face in surface.faces)
                        for (int iVertex = 0; iVertex < face.vertices.Length; iVertex++)
                            if (face.vertices[iVertex] == vertex)
                                face.vertices[iVertex] = Vector3F.Add(face.vertices[iVertex], vertexOffset);
                }

                // Create surface expanded vertices list
                surface.expanded_vertices = surface.faces.SelectMany(face => face.vertices).Distinct().ToArray();
            }
                    
            // Compute face equation and mask size
            foreach (var surface in surfaceList)
            {
                // Get the points that are farther apart
                float      Best_Mark = 0;
                Vector3F   Best_Pivot = new Vector3F();
                EdgeInfo[] Best_Edges = null;
                foreach (var pivotVert in surface.vertexEdges)
                {
                    // Get external edges list
                    var edges = pivotVert.Value.Where(t => t.normals.Count == 1).ToArray();
                    if (edges.Length != 2)
                        continue;

                    // Compute mark
                    var edge1 = Vector3F.Subtract(edges[0].p1, edges[0].p2);
                    var edge2 = Vector3F.Subtract(edges[1].p1, edges[1].p2);
                    var mark = (1 - Math.Abs(Vector3F.DotProduct(edge1.GetUnit(), edge2.GetUnit())));// *edge1.GetLengthSquared() * edge2.GetLengthSquared();

                    // Does this bet the beat result?
                    if (mark > Best_Mark)
                    {
                        Best_Mark = mark;
                        Best_Pivot = pivotVert.Key;
                        Best_Edges = edges;
                    }
                }


                surface.anchors = new Vector3F[] { Best_Pivot, 
                                                    Best_Edges[0].p1 == Best_Pivot ? Best_Edges[0].p2 : Best_Edges[0].p1,
                                                    Best_Edges[1].p1 == Best_Pivot ? Best_Edges[1].p2 : Best_Edges[1].p1};

                // Define initail surface equation
                var B = surface.anchors[0];
                var E0 = Vector3F.Subtract(surface.anchors[1], B);
                var E1 = Vector3F.Subtract(surface.anchors[2], B);
                var Normal = Vector3F.CrossProduct(E0, E1).GetUnit();

                // Should we flip the normal?
                //if (E0.GetLengthSquared() > E1.GetLengthSquared())
                if (Vector3F.DotProduct(surface.faces[0].computedNormal, Normal) < 0.5)
                {
                    var temp = E0;
                    E0 = E1;
                    E1 = temp;

                    // Recompute normal
                    Normal = Vector3F.CrossProduct(E0, E1).GetUnit();
                }

                // Normalize edges
                E0 = Vector3F.CrossProduct(E1, Normal).GetUnit();
                E1.Normalize();

                // Get surface space 2d bounding box
                var minS = float.MaxValue; var maxS = float.MinValue;
                var minT = float.MaxValue; var maxT = float.MinValue;
                for (int iv = 0; iv < surface.expanded_vertices.Length; iv++)
                {
                    var res = PointQuadDistance(surface.expanded_vertices[iv], B, E0, E1);
                    if (res.s > maxS) maxS = res.s;
                    if (res.s < minS) minS = res.s;
                    if (res.t > maxT) maxT = res.t;
                    if (res.t < minT) minT = res.t;
                }

                // Scale edges so the entire polygoin will fit 0..1
                surface.surface_B  = B  = Vector3F.Add(B, Vector3F.Add(Vector3F.Multiply(E0, minS), Vector3F.Multiply(E1, minT)));
                surface.surface_E0 = E0 = Vector3F.Multiply(E0, (maxS - minS));
                surface.surface_E1 = E1 = Vector3F.Multiply(E1, (maxT - minT));

                // Compute image size
                surface.maskSize = new Size(512, (int)(512 * E0.GetLength() / E1.GetLength()));
            }

            // Create main mask image
            var totalHeight = surfaceList.Sum(t => t.maskSize.Height);
            var maskImg = new Bitmap(512, totalHeight, PixelFormat.Format24bppRgb);
            var mask_g = Graphics.FromImage(maskImg);
            mask_g.FillRectangle(Brushes.Black, new Rectangle(0, 0, maskImg.Width, maskImg.Height));

            var topPos = 0;

            // Draw mask and debug images
            int counter = 0;
            foreach (var surface in surfaceList)
            {
                // Skip empty suurfaces
                if (surface.maskSize.Height == 0)
                    continue;

                // Cache things
                var B = surface.surface_B;
                var E0 = surface.surface_E0;
                var E1 = surface.surface_E1;

                // Define debug image size
                var dbgImgBorder = 10;
                var dbgImgSize = new Size(surface.maskSize.Width + dbgImgBorder * 2, surface.maskSize.Height + dbgImgBorder * 2 + 40);
                var dbgImgRect = new Rectangle(ImgBorder, ImgBorder, surface.maskSize.Width, surface.maskSize.Height);

                // Define mask position
                var maskImgRect = new Rectangle(0, topPos, surface.maskSize.Width, surface.maskSize.Height);

                glslCode += string.Format("future = BouncePointQuad(positions[i].xyz, future, (float3)({0:+d;-d;0}, {1}, {2}), (float3)({3}, {4}, {5}), (float3)({6}, {7}, {8}), surfacesMask, {9}, {10}, edgeOffset);\r\n",
                    F2S(B.X),  F2S(B.Y),  F2S(B.Z),
                    F2S(E0.X), F2S(E0.Y), F2S(E0.Z),
                    F2S(E1.X), F2S(E1.Y), F2S(E1.Z),
                    topPos, surface.maskSize.Height);

                // Move position in mask
                topPos += surface.maskSize.Height;

                // Draw triangles
                var dbgImage = new Bitmap(dbgImgRect.Width + ImgBorder * 2, dbgImgRect.Height + ImgBorder * 2 + 40);
                var g = Graphics.FromImage(dbgImage);
                g.FillRectangle(Brushes.White, new Rectangle(0, 0, dbgImage.Width, dbgImage.Height));
                foreach (var face in surface.faces)
                {
                    // Draw debug
                    var points = new[] {
                        PointInQuad2D(face.vertices[0], B, E0, E1, dbgImgRect),
                        PointInQuad2D(face.vertices[1], B, E0, E1, dbgImgRect),
                        PointInQuad2D(face.vertices[2], B, E0, E1, dbgImgRect) };

                    g.FillPolygon(Brushes.Pink, points);

                    // Draw mask
                    var maskPoints = new[] {
                        PointInQuad2D(face.vertices[0], B, E0, E1, maskImgRect),
                        PointInQuad2D(face.vertices[1], B, E0, E1, maskImgRect),
                        PointInQuad2D(face.vertices[2], B, E0, E1, maskImgRect) };

                    mask_g.FillPolygon(Brushes.White, maskPoints);

                    mask_g.DrawPolygon(Pens.LightGray, maskPoints);
                }

                // Draw anchor points
                var qs1 = PointInQuad2D(surface.anchors[0], B, E0, E1, dbgImgRect);
                var qs2 = PointInQuad2D(surface.anchors[1], B, E0, E1, dbgImgRect);
                var qs3 = PointInQuad2D(surface.anchors[2], B, E0, E1, dbgImgRect);
                //g.DrawEllipse(Pens.Green, qs1.X - 2, qs1.Y - 2, 5, 5);
                //g.DrawEllipse(Pens.Green, qs2.X - 2, qs2.Y - 2, 5, 5);
                //g.DrawEllipse(Pens.Green, qs3.X - 2, qs3.Y - 2, 5, 5);
                var fnt = new Font("Courier New", 10.0f, FontStyle.Regular);
                g.DrawString("B", fnt, Brushes.DarkBlue, qs1);
                g.DrawString("E0(s)", fnt, Brushes.DarkBlue, qs2);
                g.DrawString("E1(t)", fnt, Brushes.DarkBlue, qs3);

                // Draw Edges
                foreach (var edge in surface.edges)
                {
                    // Check if an  inner edge
                    var innerEdge = (edge.normals.Count > 1);

                    // Draw Edge
                    g.DrawLine(innerEdge ? Pens.Red : Pens.Green, PointInQuad2D(edge.p1, B, E0, E1, dbgImgRect), PointInQuad2D(edge.p2, B, E0, E1, dbgImgRect));

                    // Draw Edge normal
                    if (!innerEdge)
                    {
                        var mid = Vector3F.Multiply(Vector3F.Add(edge.p1, edge.p2), 0.5f);
                        var norm = Vector3F.Add(mid, Vector3F.Multiply(edge.normals[0], 0.2f));
                        g.DrawLine(Pens.Purple, PointInQuad2D(mid, B, E0, E1, dbgImgRect), PointInQuad2D(norm, B, E0, E1, dbgImgRect));
                    }
                }

                //var fnt = new Font("Courier New", 10.0f, FontStyle.Regular);
                foreach (var vertex in surface.vertices)
                {
                    var pnt = PointInQuad2D(vertex, B, E0, E1, dbgImgRect);
                    //g.DrawString(surface.vertexEdges[vertex].Count.ToString(), fnt, Brushes.DarkCyan, pnt);
                }

                g.DrawString("v0 = " + obj.positions[surface.faces[0].positionIdx[0] - 1].ToString(), fnt, Brushes.Black, new PointF(10, dbgImage.Height - 50));
                g.DrawString("B  = " + B.ToString(), fnt, Brushes.Black, new PointF(10, dbgImage.Height - 40));
                g.DrawString("E0 = " + E0.ToString(), fnt, Brushes.Black, new PointF(10, dbgImage.Height - 30));
                g.DrawString("E1 = " + E1.ToString(), fnt, Brushes.Black, new PointF(10, dbgImage.Height - 20));

                g.Dispose();
                dbgImage.Save(Path.Combine(debugImages, "face_" + counter++ + ".bmp"), ImageFormat.Bmp);
            }

            mask_g.Dispose();

            // Write equations
            File.WriteAllText(equOutput, glslCode);

            // Write mask bitmap
            maskImg.Save(maskOutput, ImageFormat.Png);
        }
    }
}
