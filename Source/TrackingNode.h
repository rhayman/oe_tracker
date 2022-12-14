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

#ifndef TRACKINGNODE_H
#define TRACKINGNODE_H

#include <ProcessorHeaders.h>
#include "TrackingMessage.h"
#include "../../../plugin-GUI/Source/Utils/Utils.h"

#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/ip/IpEndpointName.h"
#include "oscpack/osc/OscReceivedElements.h"
#include "oscpack/osc/OscPacketListener.h"
#include "oscpack/ip/UdpSocket.h"

#include <stdio.h>
#include <queue>
#include <utility>

#define BUFFER_SIZE 4096
#define MAX_SOURCES 10
#define DEF_PORT 27020
#define DEF_ADDRESS "/red"
#define DEF_COLOR "red"

inline StringArray colors = {"red",
							 "green",
							 "blue",
							 "magenta",
							 "cyan",
							 "orange",
							 "pink",
							 "grey",
							 "violet",
							 "yellow"};

auto const desc_name = std::make_unique<MetadataDescriptor>(
	MetadataDescriptor::MetadataType::CHAR,
	64,
	"Source name",
	"Tracking source name",
	"external.tracking.name");

auto const desc_port = std::make_unique<MetadataDescriptor>(
	MetadataDescriptor::MetadataType::CHAR,
	16,
	"Source port",
	"Tracking source port",
	"external.tracking.port");

auto const desc_address = std::make_unique<MetadataDescriptor>(
	MetadataDescriptor::MetadataType::CHAR,
	16,
	"Source address",
	"Tracking source address",
	"external.tracking.address");

auto const desc_position = std::make_unique<MetadataDescriptor>(
	MetadataDescriptor::MetadataType::FLOAT,
	4,
	"Source position",
	"Tracking  position",
	"external.tracking.position");

auto const desc_color = std::make_unique<MetadataDescriptor>(
	MetadataDescriptor::MetadataType::CHAR,
	16,
	"Source color",
	"Tracking source color",
	"external.tracking.color");

//	This helper class allows stores input tracking data in a circular queue.
class TrackingQueue
{
public:
	TrackingQueue();
	~TrackingQueue();

	void push(const TrackingData &message);
	TrackingData *pop();

	bool isEmpty();
	void clear();

	int count();

private:
	TrackingData m_buffer[BUFFER_SIZE];
	int m_head;
	int m_tail;
	int _count = 0;
};

//	This helper class is an OSC server running its own thread to keep data transmission
//	continuous.

class TrackingNode;

class TrackingServer : public osc::OscPacketListener,
					   public Thread
{
public:
	TrackingServer();
	TrackingServer(String port, String address);
	~TrackingServer();

	void run();
	void stop();

	void addProcessor(TrackingNode *processor);
	void removeProcessor(TrackingNode *processor);

protected:
	virtual void ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName &);

private:
	TrackingServer(TrackingServer const &);
	void operator=(TrackingServer const &);

	String m_incomingPort;
	String m_address;

	UdpListeningReceiveSocket *m_listeningSocket = nullptr;
	std::vector<TrackingNode *> m_processors;
};

class TrackingModule
{
public:
	TrackingModule() {}
	TrackingModule(String port, String address, String color, TrackingNode *processor)
		: m_port(port), m_address(address), m_color(color), m_messageQueue(std::make_unique<TrackingQueue>()), m_server(std::make_unique<TrackingServer>(port, address))
	{
		// m_server->addProcessor(processor);
		// m_server->startThread();
	}
	~TrackingModule() {}
	friend std::ostream &operator<<(std::ostream &, const TrackingModule &);
	String m_name;
	String m_port = String(DEF_PORT);
	String m_address = String(DEF_ADDRESS);
	String m_color = String(DEF_COLOR);
	std::unique_ptr<TrackingQueue> m_messageQueue = nullptr;
	std::unique_ptr<TrackingServer> m_server = nullptr;
	EventChannel *eventChannel;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackingModule);
};

class TrackingNodeSettings
{
private:
public:
	TrackingNodeSettings()
	{
		meta_position = std::make_unique<MetadataValue>(*desc_position);
	};
	TTLEventPtr createEvent(int idx, int64 sample_number);

	std::unique_ptr<MetadataValue> meta_position = nullptr;
	OwnedArray<TrackingModule> trackers;
	bool removeTracker(const String & moduleToRemove);
	int getPort(int idx) { return trackers[idx]->m_port.getIntValue(); }
	String getName(int idx) { return trackers[idx]->m_name; }
	String getAddress(int idx) { return trackers[idx]->m_address; }
	void updateTracker(int idx, Parameter *param, juce::var value);
	void clearQueue(int idx) {
		trackers[idx]->m_messageQueue->clear();
	};
	void pushMessage(int idx, TrackingData msg) {
		trackers[idx]->m_messageQueue->push(msg);
	}
};

class TrackingNode : public GenericProcessor
{
private:
	int64 m_startingRecTimeMillis;
	int64 m_startingAcqTimeMillis;

	CriticalSection lock;

	bool m_positionIsUpdated;
	bool m_isRecordingTimeLogged;
	bool m_isAcquisitionTimeLogged;
	bool m_isInitialized = false;
	int m_received_msg;

	StreamSettings<TrackingNodeSettings> settings;

	MetadataValueArray m_metadata;
	MetadataValue* meta_position;
	MetadataValue* meta_port;
	MetadataValue* meta_name;
	MetadataValue* meta_address;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackingNode);

public:
	/** The class constructor, used to initialize any members. */
	TrackingNode();

	/** The class destructor, used to deallocate memory */
	~TrackingNode() {}

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor *createEditor() override;

	void initialize(bool signalChainIsLoading) override;

	void addTracker(String moduleName, String port="", String address="", String color="");

	void removeTracker(const String &moduleName);

	void parameterValueChanged(Parameter *param) override;

	String getParameterValue(Parameter *);

	/** Called every time the settings of an upstream plugin are changed.
		Allows the processor to handle variations in the channel configuration or any other parameter
		passed through signal chain. The processor can use this function to modify channel objects that
		will be passed to downstream plugins. */
	void updateSettings() override;

	/** Defines the functionality of the processor.
		The process method is called every time a new data buffer is available.
		Visualizer plugins typically use this method to send data to the canvas for display purposes */
	void process(AudioBuffer<float> &buffer) override;

	/** Saving custom settings to XML. This method is not needed to save the state of
		Parameter objects */
	void saveCustomParametersToXml(XmlElement *parentElement) override;

	/** Load custom settings from XML. This method is not needed to load the state of
		Parameter objects*/
	void loadCustomParametersFromXml(XmlElement *parentElement) override;

	// receives a message from the osc server
	void receiveMessage(int port, String address, const TrackingData &message);
};

#endif