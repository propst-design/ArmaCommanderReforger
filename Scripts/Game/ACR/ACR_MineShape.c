// Minefield shapes (#200): a commander's 2 to 4 map points become the field's outline. Two points are a
// line, a band LINE_HALF either side of it; three or four are that area. The same mines are spread
// over it (the price stays with the mines, not the shape), so it may be no bigger than MAX_AREA and no
// longer than MAX_SPAN corner to corner. Corners clicked in any order make a plain shape: they're
// sorted round their middle, so four corners never cross into a bow-tie. Flat geometry on x and z.
class ACR_MineShape
{
 static const int MOST_CORNERS = 4;
 static const float MAX_SPAN = 120;     // m, corner to corner
 static const float MAX_AREA = 2700;    // m², three times the 30 m square: the same mines no thinner than this
 static const float MIN_AREA = 80;      // m², below it the corners are as good as one point
 static const float LINE_HALF = 3;      // m either side of a line's middle
 static const float LINE_MIN = 10;      // m, a line's shortest length

 // The outline for 'corners' in 'outline' (a line's band, or the corners in order round their middle), or
 // why not.
 static string Outline(notnull array<vector> corners, notnull array<vector> outline)
 {
  outline.Clear();
  int count = corners.Count();
  if (count < 2 || count > MOST_CORNERS) return string.Format("a minefield takes 1 to %1 points", MOST_CORNERS);
  foreach (vector corner : corners)
  {
   // NaN fails both comparisons, so it's refused too.
   if (!(Math.AbsFloat(corner[0]) < 1000000) || !(Math.AbsFloat(corner[2]) < 1000000)) return "those points are off the map";
  }
  if (ACR_MineShape.CornerSpan(corners) > MAX_SPAN) return string.Format("too long: at most %1 m from corner to corner", (int)MAX_SPAN);
  if (count == 2)
  {
   float length = vector.DistanceXZ(corners[0], corners[1]);
   if (length < LINE_MIN) return string.Format("too short for a line: at least %1 m", (int)LINE_MIN);
   ACR_MineShape.Band(corners[0], corners[1], LINE_HALF, outline);
   return "";
  }
  outline.Copy(corners);
  ACR_MineShape.Order(outline);
  float area = ACR_MineShape.Area(outline);
  if (area < MIN_AREA) return "too narrow: spread the corners further apart";
  if (area > MAX_AREA) return string.Format("too big for its mines: at most %1 m² (this is %2 m²)", (int)MAX_AREA, (int)area);
  return "";
 }

 // The corners in order round their middle (in place).
 static void Order(notnull array<vector> corners)
 {
  vector middle = ACR_MineShape.Middle(corners);
  array<float> angles = {};
  foreach (vector corner : corners) angles.Insert(Math.Atan2(corner[2] - middle[2], corner[0] - middle[0]));
  for (int i = 1; i < corners.Count(); i++)
  {
   for (int j = i; j > 0 && angles[j] < angles[j - 1]; j--)
   {
    float a = angles[j];
    angles[j] = angles[j - 1];
    angles[j - 1] = a;
    vector c = corners[j];
    corners[j] = corners[j - 1];
    corners[j - 1] = c;
   }
  }
 }

 // The average of the points.
 static vector Middle(notnull array<vector> points)
 {
  vector sum;
  foreach (vector p : points) sum = sum + p;
  if (points.IsEmpty()) return sum;
  return sum * (1.0 / points.Count());
 }

 // Flat area of the outline (shoelace), m².
 static float Area(notnull array<vector> outline)
 {
  int count = outline.Count();
  if (count < 3) return 0;
  float twice;
  vector o = outline[0];
  for (int i = 0; i < count; i++)
  {
   vector a = outline[i];
   vector b = outline[(i + 1) % count];
   twice += (a[0] - o[0]) * (b[2] - o[2]) - (b[0] - o[0]) * (a[2] - o[2]);
  }
  return Math.AbsFloat(twice) * 0.5;
 }

 // Whether p is inside the outline (flat, crossing count).
 static bool Inside(notnull array<vector> outline, vector p)
 {
  int count = outline.Count();
  if (count < 3) return false;
  bool inside = false;
  int j = count - 1;
  for (int i = 0; i < count; i++)
  {
   vector a = outline[i];
   vector b = outline[j];
   bool crosses = (a[2] > p[2]) != (b[2] > p[2]);
   if (crosses && p[0] < (b[0] - a[0]) * (p[2] - a[2]) / (b[2] - a[2]) + a[0]) inside = !inside;
   j = i;
  }
  return inside;
 }

 // The longest distance between two of the points, m (not "Span": a mod in the playtest set has one).
 static float CornerSpan(notnull array<vector> points)
 {
  float most;
  for (int i = 0; i < points.Count(); i++)
  {
   for (int j = i + 1; j < points.Count(); j++) most = Math.Max(most, vector.DistanceXZ(points[i], points[j]));
  }
  return most;
 }

 // The band 'half' m either side of the line from a to b, as an outline of four corners.
 static void Band(vector a, vector b, float half, notnull array<vector> outline)
 {
  vector along = b - a;
  along[1] = 0;
  if (along.Length() < 0.01) along = Vector(0, 0, 1);
  along.Normalize();
  vector side = Vector(along[2], 0, -along[0]) * half;
  outline.Clear();
  outline.Insert(a + side);
  outline.Insert(b + side);
  outline.Insert(b - side);
  outline.Insert(a - side);
 }

 // The direction of the outline's longest edge (a line's own direction), flat and normalized.
 static vector Long(notnull array<vector> outline)
 {
  vector best = Vector(0, 0, 1);
  float longest = -1;
  int count = outline.Count();
  for (int i = 0; i < count; i++)
  {
   vector edge = outline[(i + 1) % count] - outline[i];
   edge[1] = 0;
   float length = edge.Length();
   if (length <= longest) continue;
   longest = length;
   best = edge;
  }
  if (best.Length() < 0.01) return Vector(0, 0, 1);
  best.Normalize();
  return best;
 }

 // The farthest any outline point is from 'middle', m.
 static float Reach(notnull array<vector> outline, vector middle)
 {
  float most;
  foreach (vector p : outline) most = Math.Max(most, vector.DistanceXZ(p, middle));
  return most;
 }

 // The flat distance from p to the outline's nearest edge, m (0 or more; inside or out).
 static float EdgeDistance(notnull array<vector> outline, vector p)
 {
  float best = float.MAX;
  int count = outline.Count();
  for (int i = 0; i < count; i++)
  {
   vector a = outline[i];
   vector b = outline[(i + 1) % count];
   float abx = b[0] - a[0];
   float abz = b[2] - a[2];
   float lengthSq = abx * abx + abz * abz;
   float t = 0;
   if (lengthSq > 0.0001) t = Math.Clamp(((p[0] - a[0]) * abx + (p[2] - a[2]) * abz) / lengthSq, 0, 1);
   float dx = a[0] + abx * t - p[0];
   float dz = a[2] + abz * t - p[2];
   best = Math.Min(best, Math.Sqrt(dx * dx + dz * dz));
  }
  return best;
 }

 // Whether p is inside the outline or within 'margin' m of it.
 static bool Near(notnull array<vector> outline, vector p, float margin)
 {
  return ACR_MineShape.Inside(outline, p) || ACR_MineShape.EdgeDistance(outline, p) <= margin;
 }

 // The outline's flat bounds.
 static void Bounds(notnull array<vector> outline, out float minX, out float minZ, out float maxX, out float maxZ)
 {
  minX = float.MAX;
  minZ = float.MAX;
  maxX = -float.MAX;
  maxZ = -float.MAX;
  foreach (vector p : outline)
  {
   minX = Math.Min(minX, p[0]);
   minZ = Math.Min(minZ, p[2]);
   maxX = Math.Max(maxX, p[0]);
   maxZ = Math.Max(maxZ, p[2]);
  }
 }
}
