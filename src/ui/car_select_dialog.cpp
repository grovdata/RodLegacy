#include "car_select_dialog.h"
#include "../state.h"

CarSelectDialog::CarSelectDialog(std::function<void(int)> onCarSelected)
    : Dialog("Your cars", {36, 26, 246, 136}, state.assets.GetMainFont()),
      onCarSelected(onCarSelected)
{
 
    listBoxRect = {3, listRowHeight + 6, 242, 80};
    EnableList();
    
    int buttonWidth = 66;
    int buttonHeight = 16;

    Button* switchItButton = new Button(
        "Switch it",
        SDL_Rect{bounds.x + 14, bounds.y + bounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
        [this]() {
            if (selectedIndex >= 0) {
                this->OnItemSelected(selectedIndex);
            }
        }
    );

    Button* sellItButton = new Button(
        "Sell it",
        SDL_Rect{bounds.x + 14 + buttonWidth + 2, bounds.y + bounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
        [this]() {
            // TODO: Implement sell logic
        }
    );
    
    Button* cancelBtn = new Button(
        "Forget it",
        SDL_Rect{bounds.x + 14 + buttonWidth*2 + 2*2, bounds.y + bounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
        [this]() { Hide(); }
    );
    
    AddButton(switchItButton);
    AddButton(sellItButton);
    AddButton(cancelBtn);    

    // Create scroll buttons
    scrollUpBtn = new Button(
        "/",
        SDL_Rect{bounds.x + bounds.w - 20, bounds.y + 5, 15, 15},
        [this]() {
            if (scrollOffset > 0) scrollOffset--;
        }
    );
    
    scrollDownBtn = new Button(
        ">", 
        SDL_Rect{bounds.x + bounds.w - 20, bounds.y + bounds.h - 40, 15, 15},
        [this]() {
            if (scrollOffset + visibleItems < static_cast<int>(items.size())) scrollOffset++;
        }
    );
    
    AddButton(scrollUpBtn);
    AddButton(scrollDownBtn);
    
    PopulateCarList();
}

void CarSelectDialog::PopulateCarList() {
    std::vector<std::string> carNames;
    carNames.reserve(state.GetOwnedCars().size());
    for (const auto& model : state.GetOwnedCars()) {
        carNames.push_back(model->GetModel().name);
    }
    SetItems(carNames);
}

void CarSelectDialog::OnItemSelected(int index) {
    if (onCarSelected) {
        onCarSelected(index);
    }
    Hide();
} 