
import line from '@line/bot-sdk';
import ngrok from 'ngrok';
import express from 'express';
import axios from 'axios';
import mqtt from 'mqtt';
import dotenv from 'dotenv';
dotenv.config()
import { initializeApp } from "firebase/app";
import {getFirestore, collection, getDoc ,doc, setDoc, getDocs} from "firebase/firestore";


// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
// For Firebase JS SDK v7.20.0 and later, measurementId is optional
const firebaseConfig = {
  apiKey: "AIzaSyBwymRc-CxX53AkPVDNh1MpG5MfaIrkEMg",
  authDomain: "backache-8099c.firebaseapp.com",
  projectId: "backache-8099c",
  storageBucket: "backache-8099c.appspot.com",
  messagingSenderId: "1056971717520",
  appId: "1:1056971717520:web:27cde6b67ea1c78d6f5115",
  measurementId: "G-RJCFLYGPWH"
};

const config = {
  channelAccessToken: process.env.CHANNEL_ACCESS_TOKEN,
  channelSecret: process.env.CHANNEL_SECRET,
};

//console.log(config);
const client = new line.Client(config);
const app = express();

// Initialize Firebase
const firebaseApp = initializeApp(firebaseConfig); 
const db = getFirestore(); 
async function  set(nameCollection,nameDocument, data) { 
  try {          
   await setDoc(doc(db, nameCollection, nameDocument), data);                 
      console.log("Document written with ID: ", nameDocument);        
  } 
  catch (e) {      
      console.error("Error adding document: ", e); 
  } 
}

let payloads = {'text' : 'text'};
var mqttClient = mqtt.connect('mqtt://broker.hivemq.com');

// เชื่อมต่อ
mqttClient.on('connect', () => {
     console.log('connected')
     mqttClient.subscribe(['cn466/natthaporn/backache/#'], () => {
         console.log("Topic subscribed")
    });
});


//การรับค่า
let startTime = "";
let endTime = "";
var newTime = "";

mqttClient.on('message', (topic, payload) => {
    console.log('Received Message:', topic, payload.toString());
    payloads = JSON.parse(payload.toString());
    if(payloads.newStatus == "walking"){
      var Starttoday = new Date();
      startTime = Starttoday.getTime();
    }
    if(payloads.newStatus == "sleeping"){
      var Endtoday = new Date();
      endTime = Endtoday.getTime();
      var distance = endTime - startTime;
      console.log("distance => "+distance);
      getUpdate(distance);
    }
    sendAuto(payloads.newStatus);
    console.log("Status : " + payloads.newStatus);
});

app.post('/callback', line.middleware(config), (req, res) => {
    if (req.body.destination) {
        console.log("Destination User ID: " + req.body.destination);
    }
    Promise
      .all(req.body.events.map(handleEvent))
      .then((result) => res.json(result))
      .catch((err) => {
        console.log("error jaaaaaaaaaaaaaaaaaaaaaaa")
        console.error(err);
        res.status(500).end();
    });
});

// get data to show on html
app.get("/home",  async (req, res) => {
  const querySnapshot = await getDocs(collection(db, "Data"));
  const totalData = [];
  querySnapshot.forEach((doc) => {
    let totaltime = doc.data().TotalTime;
    let hours_m = Math.floor((totaltime % (1000 * 60 * 60 * 24)) / (1000 * 60));
    let minutes = Math.floor((totaltime % (1000 * 60 * 60)) / (1000 * 60));
    let seconds = Math.floor((totaltime % (1000 * 60)) / 1000);
    totalData.push({"Date": doc.id ,"Time": `${hours_m+minutes} นาที ${seconds} วินาที`});
  });
  res.json({ "Data" : totalData});
});

// get and set data and calculate
async function getUpdate(newTime){
  let today = new Date();
  let date = today.getDate()+'-'+(today.getMonth()+1)+'-'+today.getFullYear();
  const querySnapshot = await getDocs(collection(db, "Data"));
  querySnapshot.forEach((doc) => {
    if(doc.id == date){
      var oldTime = doc.data().TotalTime;
      var total = newTime + oldTime;
      set("Data" , date ,{"TotalTime": total});
      console.log(doc.id, ' => ', doc.data().TotalTime);
    }
  });
}

async function handleEvent(event) {
    let text = event.message.text  
    let newStatus = { type: 'text', text: payloads.newStatus}; 
    if (event.type !== 'message' || event.message.type !== 'text') {
      // ignore non-text-message event
      return Promise.resolve(null);
    }
    if(text == 'status'){
      return client.replyMessage(event.replyToken, newStatus);
    } 
    if(text == 'time'){
      let total_min = 0;
      let seconds =0;
      let today = new Date();
      let date = today.getDate()+'-'+(today.getMonth()+1)+'-'+today.getFullYear();
      const querySnapshot = await getDocs(collection(db, "Data"));
      querySnapshot.forEach((doc) => {
      if(doc.id == date){
        let totaltime = doc.data().TotalTime;
        let hours_m = Math.floor((totaltime % (1000 * 60 * 60 * 24)) / (1000 * 60));
        let minutes = Math.floor((totaltime % (1000 * 60 * 60)) / (1000 * 60));
        seconds = Math.floor((totaltime % (1000 * 60)) / 1000);
        total_min = hours_m + minutes;
        
        console.log(doc.id, ' => ', doc.data().TotalTime);
      }
  }); 
  return client.replyMessage(event.replyToken, { type: 'text', text: `${total_min} นาที ${seconds} วินาที`});
      // console.log("kkkkkk => "+totalTime);
      // return client.replyMessage(event.replyToken, gg);
    } 
    if( text == 'setDate'){
      var today = new Date();
      var date = today.getDate()+'-'+(today.getMonth()+1)+'-'+today.getFullYear();
      var time = today.getHours() + ":" + today.getMinutes() + ":" + today.getSeconds();
      set("Data" , date ,{"TotalTime": 0});
    }
    return client.replyMessage(event.replyToken, newStatus); 
}

async function sendAuto(msg){
  return client.broadcast({ type: 'text', text: `เปลี่ยนการขยับร่างกายเป็น ${payloads.newStatus}`}); 
}

// initialization
async function initServices() {
    const baseURL = await ngrok.connect(port);
    console.log('Set LINE webhook at ' + baseURL + '/callback');
    await client.setWebhookEndpointUrl(baseURL + '/callback');
}

const port = process.env.PORT || 3000;
async function start_ngrok() {  
    //const url =  process.env.BASE_URL
    const url = await ngrok.connect(port);
    await client.setWebhookEndpointUrl(url + '/callback');
    console.log(url);
}

// LIFF UI
app.use(express.static('static'));

start_ngrok();
app.listen(port, () => {
  console.log(`listening on ${port}`);
});


