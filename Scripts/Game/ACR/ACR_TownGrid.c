// A square grid laid over a town, and the binary heap its shortest-path walks use. ACR_BaseRing (where the
// bags go) and ACR_KillGround (where he walks and what fires on it) each raster the town their own way and
// carry their own costs; the cell arithmetic, the eight neighbours and the heap are shared here.
class ACR_TownGrid : Managed
{
 int m_iN;           // cells a side
 float m_fX;         // the grid's corner
 float m_fZ;
 float m_fCell;      // m a side of a cell
 ref array<float> m_aY = {};
 protected ref array<int> m_aDX = {1, -1, 0, 0, 1, 1, -1, -1};
 protected ref array<int> m_aDZ = {0, 0, 1, -1, 1, -1, 1, -1};
 protected ref array<float> m_aStep = {1, 1, 1, 1, 1.414, 1.414, 1.414, 1.414};

 protected int At(float x, float z)
 {
  if (x < m_fX || z < m_fZ) return -1;
  int ix = (x - m_fX) / m_fCell;
  int iz = (z - m_fZ) / m_fCell;
  if (ix >= m_iN || iz >= m_iN) return -1;
  return iz * m_iN + ix;
 }

 vector Middle(int i)
 {
  int iz = i / m_iN;
  int ix = i - iz * m_iN;
  return Vector(m_fX + (ix + 0.5) * m_fCell, m_aY[i], m_fZ + (iz + 0.5) * m_fCell);
 }

 protected void HeapPush(notnull array<float> keys, notnull array<int> values, float key, int value)
 {
  int at = keys.Count();
  keys.Insert(key);
  values.Insert(value);
  while (at > 0)
  {
   int up = (at - 1) / 2;
   if (keys[up] <= key) break;
   keys[at] = keys[up];
   values[at] = values[up];
   at = up;
  }
  keys[at] = key;
  values[at] = value;
 }

 protected int HeapPop(notnull array<float> keys, notnull array<int> values, out float key)
 {
  int top = values[0];
  key = keys[0];
  int last = keys.Count() - 1;
  float moved = keys[last];
  int movedValue = values[last];
  keys.Remove(last);
  values.Remove(last);
  if (last == 0) return top;
  int at = 0;
  while (true)
  {
   int child = at * 2 + 1;
   if (child >= last) break;
   if (child + 1 < last && keys[child + 1] < keys[child]) child++;
   if (keys[child] >= moved) break;
   keys[at] = keys[child];
   values[at] = values[child];
   at = child;
  }
  keys[at] = moved;
  values[at] = movedValue;
  return top;
 }
}
