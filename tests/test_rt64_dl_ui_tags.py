"""Producer command spans stay tied to their task and override inherited tags."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class DlUiTagsTests(unittest.TestCase):
    def test_fingerprint_is_checked_before_temporary_dl_mutations(self):
        source = (Path(__file__).resolve().parents[1] /
                  "src/graphics/rt64_context.cpp").read_text()
        section = source[source.index("void send_dl("):]
        self.assertLess(section.index("ui_tag_queue.take("),
                        section.index("F3DMedalAlphaPatch medal_alpha("))
        self.assertLess(section.index("ui_tag_queue.take("),
                        section.index("F3DFlatShadePatch flat_shade("))

    def test_task_isolation_nested_lists_and_center_override(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        root = Path(__file__).resolve().parents[1]
        source = r'''
#include "tetrisphere/dl_ui_tags.h"
#include <cassert>
#include <vector>
using namespace tetrisphere;
int main() {
    std::vector<std::uint8_t> rdram(0x10000,0x5a);
    auto record=[&](DlUiTagQueue &queue, std::uint32_t first, std::uint32_t last, DlUiAnchor anchor) {
        return queue.record(first,last,anchor,rdram.data(),rdram.size());
    };
    auto take=[&](DlUiTagQueue &queue, std::uint32_t first, std::uint32_t size) {
        return queue.take(first,size,rdram.data(),rdram.size());
    };
    DlUiTagQueue q;
    assert(record(q,0x1000,0x1010,DlUiAnchor::Left));
    assert(record(q,0x1008,0x1010,DlUiAnchor::Center));
    auto first=take(q,0x1000,0x100);
    assert(first.task_generation==1 && first.spans.size()==2);
    const std::uint32_t parent_left[]={0x1004};
    assert(first.lookup(0x1004)==DlUiAnchor::Left);
    assert(first.lookup(0x100c)==DlUiAnchor::Center); // narrower result wins
    assert(first.lookup(0x5000,parent_left,1)==DlUiAnchor::Left);
    assert(record(q,0x2000,0x2010,DlUiAnchor::Right));
    auto second=take(q,0x2000,0x100);
    assert(second.task_generation==2 && second.spans.size()==1);
    assert(second.lookup(0x2004)==DlUiAnchor::Right);
    assert(second.lookup(0x1004)==DlUiAnchor::None);
    assert(q.pending_count()==0);
    // A result screen explicitly tagged Center cannot inherit a nearby left
    // HUD producer from its parent display list.
    DlUiTagQueue result;
    assert(record(result,0x3000,0x3008,DlUiAnchor::Left));
    assert(record(result,0x4000,0x4008,DlUiAnchor::Center));
    auto result_batch=take(result,0x3000,0x2000);
    const std::uint32_t result_parent[]={0x3004};
    assert(result_batch.lookup(0x4004,result_parent,1)==DlUiAnchor::Center);
    assert(result_batch.lookup(0x5000,result_parent,1)==DlUiAnchor::Left);
    // A buffer address may be reused; the old task's tags cannot survive it.
    assert(record(q,0x1000,0x1010,DlUiAnchor::Right));
    auto reused=take(q,0x1000,0x100);
    assert(reused.task_generation==3 && reused.spans.size()==1);
    assert(reused.lookup(0x1004)==DlUiAnchor::Right);
    auto empty=take(q,0x1000,0x100);
    assert(empty.spans.empty() && empty.lookup(0x1004)==DlUiAnchor::None);
    // The game fills both alternating buffers before sending the first task.
    // Keep B while A is consumed; exact bytes and address select B later.
    DlUiTagQueue alternating;
    assert(record(alternating,0x1000,0x1010,DlUiAnchor::Left));
    assert(record(alternating,0x2000,0x2010,DlUiAnchor::Right));
    auto a=take(alternating,0x1000,0x100);
    assert(a.spans.size()==1 && a.lookup(0x1004)==DlUiAnchor::Left);
    assert(alternating.pending_count()==1);
    auto b=take(alternating,0x2000,0x100);
    assert(b.spans.size()==1 && b.lookup(0x2004)==DlUiAnchor::Right);
    assert(alternating.pending_count()==0);
    // Reusing an address with different DL bytes invalidates its old tag.
    assert(record(alternating,0x2000,0x2010,DlUiAnchor::Right));
    rdram[0x2004] ^= 0xff;
    auto changed=take(alternating,0x2000,0x100);
    assert(changed.spans.empty() && alternating.pending_count()==0);
    assert(record(alternating,0x2000,0x2010,DlUiAnchor::Center));
    auto fresh=take(alternating,0x2000,0x100);
    assert(fresh.lookup(0x2004)==DlUiAnchor::Center);
    // Unmatched spans expire after a bounded number of other tasks.
    assert(record(alternating,0x2000,0x2010,DlUiAnchor::Left));
    take(alternating,0x1000,0x100);
    take(alternating,0x1000,0x100);
    take(alternating,0x1000,0x100);
    auto stale=take(alternating,0x2000,0x100);
    assert(stale.spans.empty());
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tags.cpp"
            exe = Path(directory) / "tags"
            path.write_text(source)
            compiled = subprocess.run(
                [compiler, "-std=c++17", "-I", str(root / "include"),
                 str(path), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
