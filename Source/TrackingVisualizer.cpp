/*
    ------------------------------------------------------------------

    This file is part of the Tracking plugin for the Open Ephys GUI
    Written by:

    Alessio Buccino     alessiob@ifi.uio.no
    Mikkel Lepperod
    Svenn-Arne Dragly

    Center for Integrated Neuroplasticity CINPLA
    Department of Biosciences
    University of Oslo
    Norway

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
#include "TrackingNode.h"
#include "TrackingVisualizer.h"
#include "TrackingVisualizerEditor.h"

#include <algorithm>

TrackingVisualizer::TrackingVisualizer()
    : GenericProcessor("Tracking Visual"), m_positionIsUpdated(false), m_clearTracking(false), m_isRecording(false), m_colorUpdated(false)
{
    LOGD("Created visualizer");
}

TrackingVisualizer::~TrackingVisualizer()
{
}

AudioProcessorEditor *TrackingVisualizer::createEditor()
{
    editor = std::make_unique<TrackingVisualizerEditor>(this);
    return editor.get();
}

void TrackingVisualizer::updateSettings()
{
    sources.clear();
    TrackingSources s;
    LOGD("updating");

    for (auto stream : getDataStreams())
    {
        if (stream->getName().equalsIgnoreCase("TrackingNode datastream"))
        {
            auto evtChans = stream->getEventChannels();
            for (auto chan : evtChans)
            {
                auto idx = chan->findMetadata(desc_name->getType(), desc_name->getLength(), desc_name->getIdentifier());
                auto val = chan->getMetadataValue(idx);
                String name;
                val->getValue(name);

                idx = chan->findMetadata(desc_color->getType(), desc_color->getLength(), desc_color->getIdentifier());
                val = chan->getMetadataValue(idx);
                String color;
                val->getValue(color);

                s.eventIndex = chan->getLocalIndex();
                s.sourceId = chan->getNodeId();
                s.name = name;
                s.color = color;
                s.x_pos = -1;
                s.y_pos = -1;
                s.width = -1;
                s.height = -1;
                sources.add(s);
                m_colorUpdated = true;
            }
        }
    }
    isEnabled = true;
}

void TrackingVisualizer::process(AudioSampleBuffer &)
{
    checkForEvents();
    // Clear tracking when start recording
    if (CoreServices::getRecordingStatus())
        m_isRecording = true;
    else
    {
        m_isRecording = false;
        m_clearTracking = false;
    }
}

void TrackingVisualizer::handleTTLEvent(TTLEventPtr event_ptr)
{
    DataStream * stream = getDataStream(event_ptr->getStreamId());
    if (stream->getName().equalsIgnoreCase("TrackingNode datastream"))
    {
        auto chan = event_ptr->getChannelInfo();
        auto idx = chan->findMetadata(desc_name->getType(), desc_name->getLength(), desc_name->getIdentifier());
        auto val = chan->getMetadataValue(idx);
        String name;
        val->getValue(name);
        for (auto & source : sources) {
            if (name.equalsIgnoreCase(source.name)) {
                auto nMetas = chan->getMetadataCount();
                idx = chan->findMetadata(desc_position->getType(), desc_position->getLength(), desc_position->getIdentifier());
                if ( idx != -1 ) {
                    auto val = event_ptr->getMetadataValue(0);
                    Array<float> position; // x, y , height, width
                    val->getValue(position);
                    if (!(position[0] != position[0] || position[1] != position[1]) && position[0] != 0 && position[1] != 0)
                    {
                        source.x_pos = position[0];
                        source.y_pos = position[1];
                    }
                    if (!(position[3] != position[3] || position[2] != position[2]))
                    {
                        source.width = position[3];
                        source.height = position[2];
                    }
                    auto nevents = event_ptr->getMetadataValueCount();
                    LOGD("nEvents: ", nevents);
                    auto col = event_ptr->getMetadataValue(2);
                    String sourceColor;
                    col->getValue(sourceColor);
                    LOGD("sourceColor: ", sourceColor);

                    if (source.color.compare(sourceColor) != 0)
                    {
                        source.color = sourceColor;
                        m_colorUpdated = true;
                    }
                    m_positionIsUpdated = true;
                }
            }
        }
    }
}

TrackingSources &TrackingVisualizer::getTrackingSource(int s)
{
    if (s < sources.size())
        return sources.getReference(s);
}

float TrackingVisualizer::getX(int s) const
{
    if (s < sources.size())
        return sources[s].x_pos;
    else
        return -1;
}

float TrackingVisualizer::getY(int s) const
{
    if (s < sources.size())
        return sources[s].y_pos;
    else
        return -1;
}
float TrackingVisualizer::getWidth(int s) const
{
    if (s < sources.size())
        return sources[s].width;
    else
        return -1;
}

float TrackingVisualizer::getHeight(int s) const
{
    if (s < sources.size())
        return sources[s].height;
    else
        return -1;
}

bool TrackingVisualizer::getIsRecording() const
{
    return m_isRecording;
}

bool TrackingVisualizer::getClearTracking() const
{
    return m_clearTracking;
}

bool TrackingVisualizer::getColorIsUpdated() const
{
    return m_colorUpdated;
}

void TrackingVisualizer::setColorIsUpdated(bool up)
{
    m_colorUpdated = up;
}

int TrackingVisualizer::getNSources() const
{
    return sources.size();
}

void TrackingVisualizer::clearPositionUpdated()
{
    m_positionIsUpdated = false;
}

bool TrackingVisualizer::positionIsUpdated() const
{
    return m_positionIsUpdated;
}

void TrackingVisualizer::setClearTracking(bool clear)
{
    m_clearTracking = clear;
}
