import json, sys, collections
SOURCE, OUTPUT = 7, 8
def relayout(layout, col_dx=200.0, row_dy=64.0):
    parts = layout["parts"]; conns = layout.get("connections", [])
    depth = {p["id"]: 0 for p in parts}
    for _ in range(len(parts)):
        changed = False
        for c in conns:
            u = c["from"]["id"]; v = c["to"]["id"]
            if u in depth and v in depth and depth[v] < depth[u] + 1:
                depth[v] = depth[u] + 1; changed = True
        if not changed: break
    gd = [depth[p["id"]] for p in parts if p["type"] not in (SOURCE, OUTPUT)]
    gmax = max(gd) if gd else 0
    col = {}
    for p in parts:
        if p["type"] == SOURCE: col[p["id"]] = 0
        elif p["type"] == OUTPUT: col[p["id"]] = gmax + 2
        else: col[p["id"]] = max(1, depth[p["id"]])
    cols = collections.defaultdict(list)
    for p in parts: cols[col[p["id"]]].append(p)
    for cx, plist in cols.items():
        plist.sort(key=lambda p: (p["y"], p["x"], p["id"]))
        for row, p in enumerate(plist):
            p["x"] = float(cx) * col_dx; p["y"] = float(row) * row_dy
    return layout
if __name__ == "__main__":
    for path in sys.argv[1:]:
        d = json.load(open(path)); relayout(d); json.dump(d, open(path, "w"), indent=2)
        print("relaid out", path)
