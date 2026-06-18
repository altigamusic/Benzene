#pragma once
#include "../Action.h"
#include "../uniform.h"

class DeleteUniform : public Action
{
    Uniform uniform;

  public:
    DeleteUniform(Uniform uniform);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
