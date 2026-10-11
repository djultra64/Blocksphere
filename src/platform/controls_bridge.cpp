#include "tetrisphere/controls_bridge.h"
#include <atomic>
#include <mutex>
namespace tetrisphere {
namespace {
std::mutex mutex;
std::optional<ControlsEditor> editor;
std::atomic<bool> active{false},blocked{false};
void close_if_requested() {
 if(editor && editor->closed()) {editor.reset();active.store(false);}
}
}
void open_controls_editor(const Settings& settings,ControllerFamily family) {
 std::lock_guard lock(mutex);editor.emplace(settings,family);
 blocked.store(true);active.store(true);
}
std::optional<ControlsEditor> controls_editor_snapshot() {std::lock_guard lock(mutex);return editor;}
bool controls_editor_active() {return active.load();}
bool controls_editor_blocks_game() {return blocked.load();}
void controls_editor_release_guard(bool any_input_held) {
 std::lock_guard lock(mutex);
 if(!active.load() && !any_input_held)blocked.store(false);
}
void controls_editor_command(ControlsCommand command) {
 std::lock_guard lock(mutex);if(!editor)return;
 switch(command) {
 case ControlsCommand::Previous:editor->move(-1);break;
 case ControlsCommand::Next:editor->move(1);break;
 case ControlsCommand::Left:if(editor->focused()==0)editor->cycle_family(-1);else if(editor->focused()>=ControlsEditor::Defaults)editor->move(-1);break;
 case ControlsCommand::Right:if(editor->focused()==0)editor->cycle_family(1);else if(editor->focused()>=ControlsEditor::Defaults)editor->move(1);break;
 case ControlsCommand::Activate:editor->select();break;
 case ControlsCommand::Back:editor->back();break;
 case ControlsCommand::Disconnect:if(editor->capturing())editor->back();break;
 }
 close_if_requested();
}
void controls_editor_capture(MagicControl control,ControllerFamily family) {
 std::lock_guard lock(mutex);if(editor) {editor->set_device(family);editor->capture(control);}
}
void controls_editor_device(ControllerFamily family) {std::lock_guard lock(mutex);if(editor)editor->set_device(family);}
std::optional<Settings> take_controls_apply() {std::lock_guard lock(mutex);return editor ? editor->take_apply():std::nullopt;}
void complete_controls_apply(bool success) {std::lock_guard lock(mutex);if(editor)editor->complete_apply(success);close_if_requested();}
}
