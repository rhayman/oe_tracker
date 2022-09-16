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

#ifndef TRACKINGNODEEDITOR_H
#define TRACKINGNODEEDITOR_H

#define MAX_SOURCES 10

#include <EditorHeaders.h>

class SourceSelectorControl;

class TrackingNodeEditor : public GenericEditor, public Button::Listener
{
public:
	/** Constructor */
	TrackingNodeEditor(GenericProcessor *parentNode);

	/** Destructor */
	~TrackingNodeEditor() {}

	void buttonClicked(Button *button);

	virtual void updateCustomView() override;

private:
	std::unique_ptr<UtilityButton> plusButton;
	std::unique_ptr<UtilityButton> minusButton;
	/** Generates an assertion if this class leaks */
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackingNodeEditor);
};

#endif // TrackingNodeEDITOR_H_DEFINED