using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Sharp3D.Math.Core;

namespace ProcessForcePlanes
{
    public class Face
    {
        public string material;
        public int smoothGroups;
        public int[] normalsIdx;
        public int[] positionIdx;

        // Computed fields
        public float area;
        public Vector3F computedNormal;
        public Vector3F[] vertices;
    }

    public class Mesh
    {
        public string Name;
        public List<Face> faces = new List<Face>();
    }

    public class ObjLoader
    {
        public List<Mesh> meshs = new List<Mesh>();
        public List<Vector3F> normals = new List<Vector3F>();
        public List<Vector3F> positions = new List<Vector3F>();

        public void LoadObj(string filename)
        {

            Mesh currMesh = null;
            int currSmoothGroups = 0;
            string currMaterial = "default";

            var lines = File.ReadAllLines(filename);
            foreach (var orgLine in lines)
            {
                // Skip empty lines
                if (string.IsNullOrEmpty(orgLine))
                    continue;

                // Remove comment
                var line = orgLine.Split(new[] { '#' }, 2).First().Trim();

                // Skip if empty (After comment removal)
                if (string.IsNullOrEmpty(line))
                    continue;

                // Split string into parts
                var parts = orgLine.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);

                // Process parts type
                switch (parts[0])
                {
                    case "v": // vertex
                        positions.Add(new Vector3F(parts.Skip(1).Select(t => float.Parse(t)).ToArray()));
                        break;

                    case "vn": // vertex normal
                        normals.Add(new Vector3F(parts.Skip(1).Select(t => float.Parse(t)).ToArray()));
                        break;

                    case "g": // mesh
                        currMesh = new Mesh() { Name = parts[1] };
                        meshs.Add(currMesh);
                        break;

                    case "usemtl": // use material
                        currMaterial = parts[1];
                        break;

                    case "s": // smooth group
                        currSmoothGroups = int.Parse(parts[1]);
                        break;

                    case "f": // face
                        var subparts = parts.Skip(1).Select(t => t.Split('/').Select(q => q.Trim()).ToArray()).ToArray();
                        currMesh.faces.Add(new Face()
                        {
                            material = currMaterial,
                            smoothGroups = currSmoothGroups,
                            positionIdx = subparts.Select(t => int.Parse(t[0])).ToArray(),
                            normalsIdx = subparts.Select(t => int.Parse(t[2])).ToArray()
                        });
                        break;
                }
            }
        }
    }
}
