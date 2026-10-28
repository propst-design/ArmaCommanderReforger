// -acrMapPolygonTest 1 (server-capable, ~20 s): the guard on the command map's filled shapes
// (ACR_OrderDisplay.PolygonDrawable). The shapes the map really draws (status bar halves, order
// dots, the 2IC's site discs) pass on a 1920 x 1080 canvas, also when partly off its edge; shapes
// the game can't triangulate (too few or repeated points, no area, NaN) are skipped; and so is a
// shape wholly off the canvas, like the far squads' bars 40,000 px away on a zoomed-in Everon map.
class ACR_TestMapPolygon
{
 static const int CHECKS = 4;
 static const float W = 1920;
 static const float H = 1080;

 // A circle like CanvasWidget.TessellateCircle's (which needs a client canvas).
 static array<float> Circle(float cx, float cy, float radius, int segments)
 {
  array<float> vertices = {};
  for (int i = 0; i < segments; i++)
  {
   float angle = Math.PI2 * i / segments;
   vertices.Insert(cx + Math.Cos(angle) * radius);
   vertices.Insert(cy + Math.Sin(angle) * radius);
  }
  return vertices;
 }

 // The two halves of a status bar's dark back at (x, y), as ACR_OrderDisplay.Bar makes them.
 static bool BarDrawable(float x, float y, float width, float height)
 {
  float x0 = x - 1;
  float y0 = y - 1;
  float x1 = x + 30 + 1;
  float y1 = y + 4 + 1;
  array<float> back1 = {x0, y0, x1, y0, x1, y1};
  array<float> back2 = {x0, y0, x1, y1, x0, y1};
  return ACR_OrderDisplay.PolygonDrawable(back1, width, height) && ACR_OrderDisplay.PolygonDrawable(back2, width, height);
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;

  // What the map draws on screen: bars mid-screen and at the corners, a 1 px bar fill, the 6 px
  // order dot, the 2.5 px ping dot, a 32 px 2IC disc, and dots half off the left and bottom edges.
  array<float> thinFill = {100, 100, 101, 100, 101, 104};
  string kept = "";
  if (!BarDrawable(100, 100, W, H)) kept += " bar";
  if (!BarDrawable(1, 1, W, H)) kept += " corner-bar";
  if (!BarDrawable(W - 20, H - 3, W, H)) kept += " edge-bar";
  if (!ACR_OrderDisplay.PolygonDrawable(thinFill, W, H)) kept += " 1px-fill";
  if (!ACR_OrderDisplay.PolygonDrawable(Circle(640, 360, 6, 12), W, H)) kept += " dot";
  if (!ACR_OrderDisplay.PolygonDrawable(Circle(640, 360, 2.5, 12), W, H)) kept += " ping-dot";
  if (!ACR_OrderDisplay.PolygonDrawable(Circle(640, 360, 32, 12), W, H)) kept += " site-disc";
  if (!ACR_OrderDisplay.PolygonDrawable(Circle(-3, 500, 6, 12), W, H)) kept += " left-edge-dot";
  if (!ACR_OrderDisplay.PolygonDrawable(Circle(900, H + 4, 6, 12), W, H)) kept += " bottom-edge-dot";
  ACR_TestRegress.Check("map polygons: the shapes on the canvas draw as before", kept.IsEmpty(), "rejected:" + kept);

  // Shapes the game can't triangulate.
  array<float> none = {};
  array<float> two = {10, 10, 20, 20};
  array<float> odd = {10, 10, 20, 10, 20};
  array<float> collinear = {10, 10, 20, 20, 30, 30};
  array<float> onePoint = {50, 50, 50, 50, 50, 50};
  array<float> backAndForth = {10, 10, 40, 10, 10, 10};
  array<float> flatQuad = {10, 10, 40, 10, 10, 10, 40, 10};
  array<float> sliver = {10, 10, 40, 10, 40, 10.001};
  string passed = "";
  if (ACR_OrderDisplay.PolygonDrawable(null, W, H)) passed += " null";
  if (ACR_OrderDisplay.PolygonDrawable(none, W, H)) passed += " empty";
  if (ACR_OrderDisplay.PolygonDrawable(two, W, H)) passed += " two-points";
  if (ACR_OrderDisplay.PolygonDrawable(odd, W, H)) passed += " odd-count";
  if (ACR_OrderDisplay.PolygonDrawable(collinear, W, H)) passed += " collinear";
  if (ACR_OrderDisplay.PolygonDrawable(onePoint, W, H)) passed += " one-point";
  if (ACR_OrderDisplay.PolygonDrawable(backAndForth, W, H)) passed += " back-and-forth";
  if (ACR_OrderDisplay.PolygonDrawable(flatQuad, W, H)) passed += " flat-quad";
  if (ACR_OrderDisplay.PolygonDrawable(sliver, W, H)) passed += " sliver";
  if (ACR_OrderDisplay.PolygonDrawable(Circle(640, 360, 0, 12), W, H)) passed += " zero-radius";
  ACR_TestRegress.Check("map polygons: degenerate shapes are skipped", passed.IsEmpty(), "drawn anyway:" + passed);

  // Wholly off the canvas: skipped. The canvas size unknown (0 x 0): not culled.
  string offDrawn = "";
  if (BarDrawable(40000, 500, W, H)) offDrawn += " bar-40000px";
  if (BarDrawable(-150000, -90000, W, H)) offDrawn += " bar-far-up-left";
  if (ACR_OrderDisplay.PolygonDrawable(Circle(-500, 300, 6, 12), W, H)) offDrawn += " dot-left";
  if (ACR_OrderDisplay.PolygonDrawable(Circle(900, H + 40, 6, 12), W, H)) offDrawn += " dot-below";
  if (ACR_OrderDisplay.PolygonDrawable(Circle(W + 10, 300, 6, 12), W, H)) offDrawn += " dot-just-right";
  bool unknownKept = BarDrawable(40000, 500, 0, 0) && ACR_OrderDisplay.PolygonDrawable(Circle(150000, 90000, 6, 12), 0, 0);
  ACR_TestRegress.Check("map polygons: shapes wholly off the canvas are skipped, an unknown canvas size culls nothing", offDrawn.IsEmpty() && unknownKept, string.Format("drawn anyway:%1, unknown size kept %2", offDrawn, unknownKept));

  // NaN (e.g. from a broken position) is skipped.
  float nan = Math.Sqrt(-1);
  if (nan == nan)
  {
   ACR_TestRegress.Skip("map polygons: a NaN coordinate is skipped", "Math.Sqrt(-1) gave " + nan.ToString() + ", not NaN");
  }
  else
  {
   array<float> withNan = {100, 100, 130, 100, nan, 106};
   array<float> nanFirst = {nan, 100, 130, 100, 130, 106};
   bool nanSkipped = !ACR_OrderDisplay.PolygonDrawable(withNan, W, H) && !ACR_OrderDisplay.PolygonDrawable(nanFirst, W, H) && !ACR_OrderDisplay.PolygonDrawable(nanFirst, 0, 0);
   ACR_TestRegress.Check("map polygons: a NaN coordinate is skipped", nanSkipped, "");
  }
  ACR_TestRegress.Complete("mappolygon", CHECKS);
 }
}
