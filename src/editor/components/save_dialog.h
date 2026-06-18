#pragma once

struct SaveDialog
{
    bool show = false;

    void open();
    /// <summary>Render the save dialog, and set shouldExit to true if the user clicks yes or no.</summary>
    /// <param name="shouldExit">Set to true if the user clicks yes or no, false if the user clicks cancel or closes the window.</param>
    /// <returns>True if the changes should be saved (user clicked yes), false if not.</returns>
    bool render(bool& shouldExit);
};
