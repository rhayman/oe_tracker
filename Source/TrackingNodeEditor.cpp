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

#include "TrackingNodeEditor.h"
#include "TrackingNode.h"
#include "../../../plugin-GUI/Source/Utils/Utils.h"

TrackingNodeEditor::TrackingNodeEditor(GenericProcessor *parentNode)
    : GenericEditor(parentNode)
{
    desiredWidth = 240;

    addComboBoxParameterEditor("Source", 55, 20);

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
    addComboBoxParameterEditor("Color", 15, 70);
}

void TrackingNodeEditor::buttonClicked(Button *btn)
{
    if (btn == plusButton.get())
    {
        // add a tracking source
        TrackingNode *processor = (TrackingNode *)getProcessor();
        auto param = processor->getParameter("Source");
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        auto oldSources = cparam->getCategories();
        String txt = "Tracking source " + String(oldSources.size() + 1);
        StringArray newSources{oldSources};
        newSources.add(txt);
        cparam->setCategories(newSources);
        processor->addModule(param->getStreamId(), txt);
        updateView();
        for (auto &pe : parameterEditors)
        {
            std::cout << "pe name: " << pe->getParameterName() << std::endl;
        }
    }
    if (btn == minusButton.get())
    {
        TrackingNode *processor = (TrackingNode *)getProcessor();
        auto param = processor->getParameter("Source");
        CategoricalParameter *cparam = (CategoricalParameter *)param;
        auto oldSources = cparam->getCategories();
        auto str = cparam->getValueAsString();
        oldSources.removeString(str);
        cparam->setCategories(oldSources);
        processor->removeModule(param->getStreamId(), str);
        updateView();
    }
}

void TrackingNodeEditor::updateCustomView()
{
    CategoricalParameter *src_param = (CategoricalParameter *)getProcessor()->getParameter("Source");
    auto src_name = src_param->getSelectedString();
    for (auto ed : parameterEditors)
    {
    }
}