const serverAddress = "";
const trainEngine = "libtrain_engine_default (unremovable)";
const trackOutput = "master";
const signals_normal = [
	"signal4a", 
	"signal18a", 
	"signal19", 
	"signal5", 
	"signal15", 
	"signal26", 
	"signal28", 
	"signal30", 
	"signal14", 
	"signal32", 
	"signal1", 
	"signal45", 
	"signal47", 
	"signal49", 
	"signal9", 
	"signal24", 
	"signal37", 
	"signal8", 
	"signal23", 
	"signal36", 
	"signal7a", 
	"signal35a", 
	"signal53a", 
	"signal22a", 
	"signal46a", 
	"signal48a", 
	"signal50a", 
	"signal11"
];
const signals_shunt = [
	"signal21", 
	"signal43", 
	"signal20", 
	"signal39", 
	"signal41"
];
var server_running = false;

function dtISOStr() {
	return (new Date()).toISOString();
}

// Asynchronous wait for a duration in milliseconds.
function wait(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
}

function logDebugPromise() {
	return $.ajax({
		type: 'POST',
		url: '/monitor/log_trackstate',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log("Success logDebugPromise");
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Error logDebugPromise");
		}
	});
}

function setPointPromise(pointID, pointAspect) {
	return $.ajax({
		type: 'POST',
		url: '/controller/set-point',
		crossDomain: true,
		data: { 'point': pointID, 'state': pointAspect },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log("set point " + pointID + " to " + pointAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Error when setting point " + pointID + " to " + pointAspect + ": " + responseData + errorThrown);
		}
	});
}

function setSignalPromise(signalID, signalAspect) {
	return $.ajax({
		type: 'POST',
		url: '/controller/set-signal',
		crossDomain: true,
		data: { 'signal': signalID, 'state': signalAspect },
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			console.log('Signal ' + signalID + ' set to ' + signalAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Error when setting signal " + signalID + " to " + signalAspect + ": " + responseData + errorThrown);
		}
	});
}

function adminSetTrainSpeedPromise(train, speed) {
	console.log(dtISOStr() + ": adminSetTrainSpeedPromise(" + train + ", " + speed + ")");
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/admin/set-dcc-train-speed',
		crossDomain: true,
		data: {
			'speed': speed,
			'train': train,
			'track-output': trackOutput
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			updateTrainStatePromise(train);
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": adminSetTrainSpeedPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function adminReleaseTrainPromise(train) {
	console.log(dtISOStr() + ": adminReleaseTrainPromise(" + train + ")");
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/admin/release-train',
		crossDomain: true,
		data: {
			'train': train
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			updateTrainStatePromise(train);
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": adminReleaseTrainPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function startupPromise() {
	$('#monitoring_status_field').text("Starting...");
	return $.ajax({
		type: 'POST',
		url: '/admin/startup',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			$('#monitoring_status_field').text("Started");
			server_running = true;
		},
		error: function (responseData, textStatus, errorThrown) {
			alert("Startup Failed");
			$('#monitoring_status_field').text("Error");
		}
	});
}

function shutdownPromise() {
	$('#monitoring_status_field').text("Shutting down...");
	return $.ajax({
		type: 'POST',
		url: '/admin/shutdown',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			$('#monitoring_status_field').text("Stopped");
			server_running = false;
		},
		error: function (responseData, textStatus, errorThrown) {
			alert("Shutdown Failed");
			$('#monitoring_status_field').text("Error");
		}
	});
}

function adjustValueField(valueFieldSelector, value) {
	$(valueFieldSelector).text(value);
}

function updateTrainStatePromise(train) {
	console.log(dtISOStr() + ": updateTrainStatePromise(" + train + ")");
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/train-state',
		crossDomain: true,
		data: {
			'train': train
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			console.log(dtISOStr() + ": updateTrainStatePromise ResponseData: " + responseData);
			//                    grabbed: no - on segment: no - on block: no - orientation: left - speed step: 0 - detected speed: 0 km/h - direction: forwards
			const regexMatches = /grabbed: (.*?) - on segment: (.*?) - on block: (.*?) - orientation: (.*?) - speed step: (.*?) - detected speed: (.*?) km\/h - direction: (.*)/g.exec(responseData);
			if (regexMatches === null) {
				console.log(dtISOStr() + ": updateTrainStatePromise RegexMatches is NULL")
				return;
			}
			const grabbed = regexMatches[1];
			const segment = regexMatches[2];
			const block = regexMatches[3];
			const orientation = regexMatches[4];
			const speedStep = regexMatches[5];
			const detSpeed = regexMatches[6];
			const direction = regexMatches[7];
			console.log(dtISOStr() + ": " + `Extracted: grabbed ${grabbed}, block ${block}, speed step ${speedStep}, detected Speed ${detSpeed}, orientation ${orientation}`);
			$('#' + train + "_grabbed_value").text(grabbed);
			$('#' + train + "_on_block_value").text(block);
			$('#' + train + "_speed_step_value").text(speedStep);
			$('#' + train + "_detected_speed_value").text(detSpeed);
			$('#' + train + "_orientation_value").text(orientation);
			$('#' + train + "_direction_value").text(direction);
			if (grabbed.toLowerCase().includes("yes")) {
				$('#' + train + "_release_btn").removeClass("disabled");
			} else {
				$('#' + train + "_release_btn").addClass("disabled");
			}
			if (parseInt(speedStep, 10) !== 0) {
				$('#' + train + "_stop_btn").removeClass("disabled");
			} else {
				$('#' + train + "_stop_btn").addClass("disabled");
			}
			$('#monitoring_status_field').text("Active");
		},
		error: (responseData, textStatus, errorThrown) => {
			const current_text = $('#monitoring_status_field').text();
			if (current_text !== "Starting..." && current_text !== "Stopped" && current_text !== "Shutting down...") {
				$('#monitoring_status_field').text("Error");
				$('#' + train + "_grabbed_value").text("-");
				$('#' + train + "_on_block_value").text("-");
				$('#' + train + "_speed_step_value").text("-");
				$('#' + train + "_detected_speed_value").text("-");
				$('#' + train + "_orientation_value").text("-");
				$('#' + train + "_direction_value").text("-");
			}
		}
	});
}

function isServerRunningPromise() {
	return $.ajax({
		type: 'POST',
		url: '/monitor/is-running',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			server_running = true;
		},
		error: function (responseData, textStatus, errorThrown) {
			server_running = false;
		}
	});
}

function updateTrainStates() {
	const trains = ["cargo_db", "cargo_bayern", "cargo_green", "regional_odeg", "regional_brengdirect"];
	for(var tr of trains) {
		updateTrainStatePromise(tr);
	}
}

function allsigstop() {
	console.log(dtISOStr() + ": allsigstop()");
	
	for(var s of signals_normal) {
		setSignalPromise(s, "aspect_stop");
	}
	for(var s of signals_shunt) {
		setSignalPromise(s, "aspect_stop");
	}
}

function allsiggo() {
	console.log(dtISOStr() + ": allsiggo()");
	
	for(var s of signals_normal) {
		setSignalPromise(s, "aspect_go");
	}
	for(var s of signals_shunt) {
		setSignalPromise(s, "aspect_shunt");
	}
}

function sit1prepare() {
	console.log(dtISOStr() + ": sit1prepare()");
	setPointPromise("point8", "normal");
	setPointPromise("point9", "normal");
	setPointPromise("point10", "normal");
	setPointPromise("point11", "normal");
	setPointPromise("point12", "normal");
	setPointPromise("point13", "normal");
	setPointPromise("point14", "normal");
	setPointPromise("point15", "normal");
	setPointPromise("point16", "normal");
	setPointPromise("point17", "normal");
	setPointPromise("point18a", "normal");
	allsiggo();
}

function sit2prepare() {
	console.log(dtISOStr() + ": sit2prepare()");
	// Point 10 reverse/branch,
	// Signal 21 aspect_shunt (/go)
	allsigstop();
	setPointPromise("point8", "normal");
	setPointPromise("point9", "normal");
	setPointPromise("point10", "reverse");
	setSignalPromise("signal19", "aspect_go");
}

function sit3prepare() {
	console.log(dtISOStr() + ": sit3prepare()");
	allsigstop();
	setSignalPromise("signal4a", "aspect_stop");
	setSignalPromise("signal18a", "aspect_stop");
	setPointPromise("point2", "reverse");
	setPointPromise("point8", "normal");
	setPointPromise("point9", "normal");
	setPointPromise("point10", "reverse");
	setPointPromise("point26", "reverse");
}

function logdebug() {
	console.log(dtISOStr() + ": logdebug()");
	logDebugPromise();
}

$(document).ready(
	async function () {
		await new Promise(r => setTimeout(r, 100));
		updateTrainStates();
		
		let updateInterval = setInterval(function() {
			isServerRunningPromise()
				.then(() => updateTrainStates());
		}, 1000);
	}
);