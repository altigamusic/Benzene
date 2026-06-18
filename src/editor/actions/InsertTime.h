#pragma once
#include "../Action.h"

class InsertTime : public Action
{
    float startTime;
    float length;

  public:
    InsertTime(float startTime, float length);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
