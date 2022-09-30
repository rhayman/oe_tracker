/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2022 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <vector>
#include "TrackingNodeEditor.h"
#include "TrackingNode.h"
#include "../../../plugin-GUI/Source/Utils/Utils.h"

TrackingNodeEditor::TrackingNodeEditor(GenericProcessor *parentNode)
    : GenericEditor(parentNode)
{
    desiredWidth = 240;

    addComboBoxParameterEditor("Name", 55, 20);

    plusButton = std::make_unique<UtilityButton>("+", titleFont);
    plusButton->addListener(this);
    plusButton->setRadius(3.0f);
    plusButton->setBounds(30, 40, 20, 20);
    addAndMakeVisible(plusButton.get());

    minusButton = std::make_unique<UtilityButton>("-", titleFont);
    minusButton->addListener(this);
    minusButton->setRadius(3.0f);
    minusButton->setBounds(5, 40, 20, 20);
    addAndMakeVisible(minusButton.get());

    addTextBoxParameterEditor("Address", 150, 70);
    addTextBoxParameterEditor("Port", 150, 20);
}

void TrackingNodeEditor::buttonClicked(Button *btn)
{
    if (btn == plusButton.get())
    {
        // add a tracking source
        TrackingNode *processor = (TrackingNode *)getProcessor();
        auto param = processor->getParameter("Name");
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        auto oldSources = cparam->getCategories();
        int iSource = 0;
        if (!oldSources.isEmpty())
        {
            std::vector<int> sources;
            for (auto str : oldSources)
            {
                String n = str.fromFirstOccurrenceOf("source ", false, true);
                sources.push_back(std::stoi(n.toStdString()));
            }
            int max = *std::max_element(sources.cbegin(), sources.cend());
            iSource = max;
        }
        String txt = "Tracking source " + String(iSource + 1);
        StringArray newSources{oldSources};
        newSources.add(txt);
        cparam->setCategories(newSources);
        processor->addTracker(txt);
        updateView();
    }
    if (btn == minusButton.get())
    {
        TrackingNode *processor = (TrackingNode *)getProcessor();
        auto param = processor->getParameter("Name");
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        auto oldSources = cparam->getCategories();
        auto str = cparam->getValueAsString();
        processor->removeTracker(str);
        oldSources.removeString(str);
        cparam->setCategories(oldSources);
        updateView();
    }
}