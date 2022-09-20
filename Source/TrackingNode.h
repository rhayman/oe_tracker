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

const MetadataDescriptor desc_name = MetadataDescriptor(
	MetadataDescriptor::MetadataType::CHAR,
	64,
	"Source name",
	"Tracking source name",
	"external.tracking.name");

const MetadataDescriptor desc_port = MetadataDescriptor(
	MetadataDescriptor::MetadataType::CHAR,
	16,
	"Source port",
	"Tracking source port",
	"external.tracking.port");

const MetadataDescriptor desc_address = MetadataDescriptor(
	MetadataDescriptor::MetadataType::CHAR,
	16,
	"Source address",
	"Tracking source address",
	"external.tracking.address");

const MetadataDescriptor desc_position = MetadataDescriptor(
	MetadataDescriptor::MetadataType::FLOAT,
	4,
	"Source position",
	"Tracking  position",
	"external.tracking.position");

const MetadataDescriptor desc_color = MetadataDescriptor(
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

private:
	TrackingData m_buffer[BUFFER_SIZE];
	int m_head;
	int m_tail;
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
	TrackingModule() { createMetaValues(); }
	TrackingModule(String name, TrackingNode *processor)
		: m_name(name), m_messageQueue(new TrackingQueue()), m_server(new TrackingServer(m_port, m_address))
	{
		createMetaValues();
		m_server->addProcessor(processor);
		m_server->startThread();
	}
	TrackingModule(String name, String port, TrackingNode *processor)
		: m_name(name), m_port(port), m_messageQueue(new TrackingQueue()), m_server(new TrackingServer(port, m_address))
	{
		createMetaValues();
		m_server->addProcessor(processor);
		m_server->startThread();
	}
	TrackingModule(String name, String port, String address, String color, TrackingNode *processor)
		: m_name(name), m_port(port), m_address(address), m_color(color), m_messageQueue(new TrackingQueue()), m_server(new TrackingServer(port, address))
	{
		createMetaValues();
		m_server->addProcessor(processor);
		m_server->startThread();
	}
	TrackingModule(TrackingNode *processor)
		: m_name(""), m_port(""), m_address(""), m_color(""), m_messageQueue(new TrackingQueue()), m_server(new TrackingServer())
	{
		createMetaValues();
	}
	~TrackingModule()
	{
		if (m_messageQueue)
		{
			cout << "Deleting message queue" << endl;
			delete m_messageQueue;
		}
		if (m_server)
		{
			m_server->stop();
			cout << "Stopping thread" << endl;
			m_server->stopThread(-1);
			cout << "Waiting for exit" << endl;
			m_server->waitForThreadToExit(-1);
			cout << "Delete server" << endl;
			delete m_server;
		}
	}
	TTLEventPtr createEvent(int64 sample_number, EventChannel *chan);
	bool alreadyExists(const String &name)
	{
		return m_name == name;
	};
	friend std::ostream &operator<<(std::ostream &stream, const TrackingModule &module);
	void createMetaValues();
	String m_name;
	String m_port = "27020";
	String m_address = "/red";
	String m_color = "red";
	// Metadata for attaching to TTL events
	std::unique_ptr<MetadataValue> meta_port;
	std::unique_ptr<MetadataValue> meta_name;
	std::unique_ptr<MetadataValue> meta_address;
	std::unique_ptr<MetadataValue> meta_color;
	std::unique_ptr<MetadataValue> meta_position;

	TrackingQueue *m_messageQueue = nullptr;
	TrackingServer *m_server = nullptr;
	MetadataValueArray m_metadata;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackingModule);
};

class TrackingNodeSettings
{
private:
public:
	TrackingNodeSettings(){};
	void addModule(String name, TrackingNode *node);
	void updateModule(String name, Parameter *param);
	EventChannel *eventChannel;
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
	int m_received_msg;

	int lastNumInputs;

	StreamSettings<TrackingNodeSettings> settings;
	EventChannel *eventChannel;

	OwnedArray<TrackingModule> trackingModules;
	StringArray sourceNames;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackingNode);

public:
	/** The class constructor, used to initialize any members. */
	TrackingNode();

	/** The class destructor, used to deallocate memory */
	~TrackingNode();

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor *createEditor() override;

	void addModule(uint16 streamID, String moduleName);

	void updateModule(uint16 streamID, String moduleName, Parameter *param);

	void removeModule(uint16 streamID, String moduleName);

	TrackingModule *getModule(const String &name);

	void parameterValueChanged(Parameter *param);

	const String getSelectedSourceName();

	/** Called every time the settings of an upstream plugin are changed.
		Allows the processor to handle variations in the channel configuration or any other parameter
		passed through signal chain. The processor can use this function to modify channel objects that
		will be passed to downstream plugins. */
	void updateSettings() override;

	/** Defines the functionality of the processor.
		The process method is called every time a new data buffer is available.
		Visualizer plugins typically use this method to send data to the canvas for display purposes */
	void process(AudioBuffer<float> &buffer) override;

	/** Handles events received by the processor
		Called automatically for each received event whenever checkForEvents() is called from
		the plugin's process() method */
	void handleTTLEvent(TTLEventPtr event) override;

	/** Saving custom settings to XML. This method is not needed to save the state of
		Parameter objects */
	void saveCustomParametersToXml(XmlElement *parentElement) override;

	/** Load custom settings from XML. This method is not needed to load the state of
		Parameter objects*/
	void loadCustomParametersFromXml(XmlElement *parentElement) override;

	// TODO: CLEAN THIS
	void receiveMessage(int port, String address, const TrackingData &message);
	int getTrackingModuleIndex(String name, int port, String address);
	void addSource(int port, String address, String color, uint16 currentStream);
	void addSource(uint16 currentStream);
	void removeSource(String name);

	int getNSources();
	bool isPortUsed(int port);
};

#endif