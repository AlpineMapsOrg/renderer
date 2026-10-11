import CoreGraphics
// usage: winid <pid>  -> prints the window number of the largest window of that process
let pid = Int32(CommandLine.arguments[1]) ?? -1
let list = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]
var best: (Int, Double) = (-1, 0)
for w in list {
    guard (w[kCGWindowOwnerPID as String] as? Int32) == pid, let b = w[kCGWindowBounds as String] as? [String: Double] else { continue }
    let area = (b["Width"] ?? 0) * (b["Height"] ?? 0)
    if area > best.1 { best = (w[kCGWindowNumber as String] as? Int ?? -1, area) }
}
print(best.0)
