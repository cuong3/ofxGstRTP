/*
 * ofxGstRTP.cpp
 *
 *  Created on: Aug 27, 2013
 *      Author: arturo
 */

#include "ofxGstXMPPRTP.h"

#ifndef TARGET_LINUX
#include "gstnicesrc.h"
#include "gstnicesink.h"
static gboolean nicesrc_plugin_init(GstPlugin * plugin){
    gst_element_register(plugin, "nicesrc", GST_RANK_NONE, GST_TYPE_NICE_SRC);
}
static gboolean nicesink_plugin_init(GstPlugin * plugin){
    gst_element_register(plugin, "nicesink", GST_RANK_NONE, GST_TYPE_NICE_SINK);
}
#endif

ofxGstXMPPRTP::ofxGstXMPPRTP()
:isControlling(false)
,videoStream(NULL)
,depthStream(NULL)
,audioStream(NULL)
,oscStream(NULL)
,videoGathered(false)
,depthGathered(false)
,audioGathered(false)
,oscGathered(false)
{
#ifndef TARGET_LINUX
    static bool plugins_registered = false;
    if(!plugins_registered){
        gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "nicesrc", strdup("nicesrc"), nicesrc_plugin_init, "1.0.4", "BSD", "libnice", "nice", "http://libnice.org");
        
        gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "nicesink", strdup("nicesink"), nicesink_plugin_init, "1.0.4", "BSD", "libnice", "nice", "http://libnice.org");
        plugins_registered = true;
    }
#endif
}

ofxGstXMPPRTP::~ofxGstXMPPRTP() {
	// TODO Auto-generated destructor stub
}

void ofxGstXMPPRTP::setup(int clientLatency){
	server.setup();
	client.setup(clientLatency);

	ofAddListener(xmpp.jingleInitiationReceived,this,&ofxGstXMPPRTP::onJingleInitiationReceived);

}

void ofxGstXMPPRTP::onJingleInitiationReceived(ofxXMPPJingleInitiation & jingle){
	ofLogNotice() << "received call from " << jingle.from;

	xmpp.ack(jingle);
	xmpp.ring(jingle);

	remoteJingle = jingle;
	isControlling = false;

	ofGstUtils::startGstMainLoop();
	nice.setup("77.72.174.165",3478,false,ofGstUtils::getGstMainLoop());

	for(size_t i=0;i<jingle.contents.size();i++){
		if(jingle.contents[i].media=="video"){
			ofLogNotice() << "adding video channel to client";
			if(!videoStream){
				videoStream = new ofxNiceStream();
				videoStream->setLogName("video");
			}
			videoStream->setup(nice,3);
			nice.addStream(videoStream);
			client.addVideoChannel(videoStream,640,480,30);
		}else if(jingle.contents[i].media=="depth"){
			ofLogNotice() << "adding depth channel to client";
			if(!depthStream){
				depthStream = new ofxNiceStream();
				depthStream->setLogName("depth");
			}
			depthStream->setup(nice,3);
			nice.addStream(depthStream);
			client.addDepthChannel(depthStream,640,480,30);
		}else if(jingle.contents[i].media=="audio"){
			ofLogNotice() << "adding audio channel to client";
			if(!audioStream){
				audioStream = new ofxNiceStream();
				audioStream->setLogName("audio");
			}
			audioStream->setup(nice,3);
			nice.addStream(audioStream);
			client.addAudioChannel(audioStream);
		}else if(jingle.contents[i].media=="osc"){
			ofLogNotice() << "adding osc channel to client";
			if(!oscStream){
				oscStream = new ofxNiceStream();
				oscStream->setLogName("osc");
			}
			oscStream->setup(nice,3);
			nice.addStream(oscStream);
			client.addOscChannel(oscStream);
		}
	}

	server.play();
	client.play();


	if(videoStream) videoStream->gatherLocalCandidates();
	if(depthStream) depthStream->gatherLocalCandidates();
	if(audioStream) audioStream->gatherLocalCandidates();
	if(oscStream) oscStream->gatherLocalCandidates();

	for(size_t i=0;i<jingle.contents.size();i++){
		ofxNiceStream * stream = NULL;
		if(jingle.contents[i].media=="video"){
			stream = videoStream;
		}else if(jingle.contents[i].media=="depth"){
			stream = depthStream;
		}else if(jingle.contents[i].media=="audio"){
			stream = audioStream;
		}else if(jingle.contents[i].media=="osc"){
			stream = oscStream;
		}
		if(stream){
			stream->setRemoteCredentials(remoteJingle.contents[i].transport.ufrag, remoteJingle.contents[i].transport.pwd);
			stream->setRemoteCandidates(remoteJingle.contents[i].transport.candidates);
		}
	}
}

void ofxGstXMPPRTP::onNiceLocalCandidatesGathered( const void * sender, vector<ofxICECandidate> & candidates){
	ofxNiceStream * stream = (ofxNiceStream*) sender;
	ofxXMPPJingleContent content;
	content.media = stream->getName();
	content.name = "telekinect";
	content.payloads.resize(1);
	if(content.media=="video"){
		content.payloads[0].clockrate=90000;
		content.payloads[0].id=96;
		content.payloads[0].name="H264";
	}else if(content.media=="depth"){
		content.payloads[0].clockrate=90000;
		content.payloads[0].id=96;
		content.payloads[0].name="H264";
	}else if(content.media=="audio"){
		content.payloads[0].clockrate=48000;
		content.payloads[0].id=96;
		content.payloads[0].name="X-GST-OPUS-DRAFT-SPITTKA-00";
	}else if(content.media=="osc"){
		content.payloads[0].clockrate=90000;
		content.payloads[0].id=96;
		content.payloads[0].name="X-GST";
	}
	content.transport.pwd= stream->getLocalPwd();
	content.transport.ufrag = stream->getLocalUFrag();
	content.transport.candidates = candidates;
	localJingle.contents.push_back(content);

	/*if(!isControlling){
		for(size_t i=0;i<remoteJingle.contents.size();i++){
			if(remoteJingle.contents[i].media==stream->getName()){
				stream->setRemoteCredentials(remoteJingle.contents[i].transport.ufrag, remoteJingle.contents[i].transport.pwd);
				stream->setRemoteCandidates(remoteJingle.contents[i].transport.candidates);
				break;
			}
		}
	}*/

	if(stream==videoStream) videoGathered = true;
	if(stream==audioStream) audioGathered = true;
	if(stream==depthStream) depthGathered = true;
	if(stream==oscStream) oscGathered = true;

	if( (videoGathered || !videoStream) && (audioGathered || !audioStream) && (depthGathered || !depthStream) && (oscGathered || !oscStream)){
		if(!isControlling){
			xmpp.acceptRTPSession(remoteJingle.from,localJingle);
		}else{
			xmpp.initiateRTP(callingTo.userName+"/"+callingTo.resource,localJingle);
		}
	}
}


void ofxGstXMPPRTP::onJingleInitiationAccepted(ofxXMPPJingleInitiation & jingle){
	cout << "session accepted setting remote candidates" << endl;
	remoteJingle = jingle;

	for(size_t i=0;i<jingle.contents.size();i++){
		if(jingle.contents[i].media=="video" && videoStream){
			videoStream->setRemoteCredentials(jingle.contents[i].transport.ufrag,jingle.contents[i].transport.pwd);
			videoStream->setRemoteCandidates(jingle.contents[i].transport.candidates);
		}else if(jingle.contents[i].media=="audio" && audioStream){
			audioStream->setRemoteCredentials(jingle.contents[i].transport.ufrag,jingle.contents[i].transport.pwd);
			audioStream->setRemoteCandidates(jingle.contents[i].transport.candidates);
		}else if(jingle.contents[i].media=="depth" && depthStream){
			depthStream->setRemoteCredentials(jingle.contents[i].transport.ufrag,jingle.contents[i].transport.pwd);
			depthStream->setRemoteCandidates(jingle.contents[i].transport.candidates);
		}else if(jingle.contents[i].media=="osc" && oscStream){
			oscStream->setRemoteCredentials(jingle.contents[i].transport.ufrag,jingle.contents[i].transport.pwd);
			oscStream->setRemoteCandidates(jingle.contents[i].transport.candidates);
		}
	}

	xmpp.ack(jingle);
}



void ofxGstXMPPRTP::connectXMPP(const string & host, const string & username, const string & pwd){
	xmpp.connect(host,username,pwd);
}

vector<ofxXMPPUser> ofxGstXMPPRTP::getFriends(){
	return xmpp.getFriends();
}

void ofxGstXMPPRTP::setShow(ofxXMPPShowState showState){
	xmpp.setShow(showState);
}

void ofxGstXMPPRTP::setStatus(const string & status){
	xmpp.setStatus(status);
}

void ofxGstXMPPRTP::sendXMPPMessage(const string & to, const string & message){
	xmpp.sendMessage(to,message);
}

void ofxGstXMPPRTP::addSendVideoChannel(int w, int h, int fps, int bitrate){
	videoStream = new ofxNiceStream;
	videoStream->setLogName("video");
	server.addVideoChannel(videoStream,w,h,fps,bitrate);
	ofAddListener(videoStream->localCandidatesGathered,this,&ofxGstXMPPRTP::onNiceLocalCandidatesGathered);
}

void ofxGstXMPPRTP::addSendDepthChannel(int w, int h, int fps, int bitrate, bool depth16){
	depthStream = new ofxNiceStream;
	depthStream->setLogName("depth");
	server.addDepthChannel(depthStream,w,h,fps,bitrate,depth16);
	ofAddListener(depthStream->localCandidatesGathered,this,&ofxGstXMPPRTP::onNiceLocalCandidatesGathered);
}

void ofxGstXMPPRTP::addSendAudioChannel(){
	audioStream = new ofxNiceStream;
	audioStream->setLogName("audio");
	server.addAudioChannel(audioStream);
	ofAddListener(audioStream->localCandidatesGathered,this,&ofxGstXMPPRTP::onNiceLocalCandidatesGathered);
}

void ofxGstXMPPRTP::addSendOscChannel(){
	oscStream = new ofxNiceStream;
	oscStream->setLogName("osc");
	server.addOscChannel(oscStream);
	ofAddListener(oscStream->localCandidatesGathered,this,&ofxGstXMPPRTP::onNiceLocalCandidatesGathered);
}

void ofxGstXMPPRTP::call(const ofxXMPPUser & user){
	callingTo = user;
	isControlling = true;

	ofGstUtils::startGstMainLoop();
	nice.setup("77.72.174.165",3478,true,ofGstUtils::getGstMainLoop());

	ofAddListener(xmpp.jingleInitiationAccepted,this,&ofxGstXMPPRTP::onJingleInitiationAccepted);

	if(videoStream){
		videoStream->setup(nice,3);
		nice.addStream(videoStream);
		client.addVideoChannel(videoStream,640,480,30);
	}
	if(audioStream){
		audioStream->setup(nice,3);
		nice.addStream(audioStream);
		client.addAudioChannel(audioStream);
	}
	if(depthStream){
		depthStream->setup(nice,3);
		nice.addStream(depthStream);
		client.addDepthChannel(depthStream,640,480,30);
	}
	if(oscStream){
		oscStream->setup(nice,3);
		nice.addStream(oscStream);
		client.addOscChannel(oscStream);
	}

	server.play();
	client.play();

	if(videoStream) videoStream->gatherLocalCandidates();
	if(depthStream) depthStream->gatherLocalCandidates();
	if(audioStream) audioStream->gatherLocalCandidates();
	if(oscStream) oscStream->gatherLocalCandidates();
}

ofxGstRTPServer & ofxGstXMPPRTP::getServer(){
	return server;
}

ofxGstRTPClient & ofxGstXMPPRTP::getClient(){
	return client;
}

ofxXMPP & ofxGstXMPPRTP::getXMPP(){
	return xmpp;
}