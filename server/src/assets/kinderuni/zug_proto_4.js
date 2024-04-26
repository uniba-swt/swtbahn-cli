var speedModifier = 1; // forward = 1, backward = -1
var currentSpeed = 0;
var trainId = "cargo_db";

const serverAddress = "";
const trainEngine = "libtrain_engine_default (unremovable)";
const trackOutput = "master";

function resetMutGlobalVarsToDefault() {
	speedModifier = 1;
	currentSpeed = 0;
}

function dtISOStr() {
	return (new Date()).toISOString();
}

// Asynchronous wait for a duration in milliseconds.
function wait(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
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
			//updateTrainStatePromise(train);
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": adminSetTrainSpeedPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function stopBtnClicked() {
	console.log(dtISOStr() + ": Stop Button clicked");
	document.getElementById("stopBtn").classList.add("disabled");
	document.getElementById("slowBtn").classList.remove("disabled");
	document.getElementById("normalBtn").classList.remove("disabled");
	document.getElementById("fastBtn").classList.remove("disabled");
	oldspeed = currentSpeed;
	currentSpeed = parseInt(document.getElementById("stopBtn").getAttribute("value"), 10);
	
	adminSetTrainSpeedPromise(trainId, currentSpeed)
		.then(() => console.log(dtISOStr() + ": StopBtnClicked - Setting train speed succeeded."))
		.catch(() => {
			console.log(dtISOStr() + ": StopBtnClicked - Setting train speed was not successful!");
			currentSpeed = oldspeed;
			enableSpeedButtonsBasedOnCurrSpeed();
		});
}

function enableSpeedButtonsBasedOnTrainOnTrackPromise() {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/train-state',
		crossDomain: true,
		data: {
			'train': this.trainId
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const regexMatch = /on block: (.*?) /g.exec(responseData);
			if (regexMatch === null) {
				console.log(dtISOStr() + ": enableSpeedButtonsBasedOnTrainOnTrackPromise regexMatch is NULL");
				disableAllSpeedButtons();
				disableDirectionButton();
				return;
			}
			const onblockVal = regexMatch[1];
			if (onblockVal === null || onblockVal.includes("no")) {
				console.log(dtISOStr() + ": enableSpeedButtonsBasedOnTrainOnTrackPromise Train not on track");
				disableAllSpeedButtons();
				disableDirectionButton();
			} else {
				console.log(dtISOStr() + ": enableSpeedButtonsBasedOnTrainOnTrackPromise Train is on track");
				enableDirectionButton();
				enableSpeedButtonsBasedOnCurrSpeed();
			}
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": enableSpeedButtonsBasedOnTrainOnTrackPromise ERR");
			disableAllSpeedButtons();
			disableDirectionButton();
		}
	});
}

function slowBtnClicked() {
	console.log(dtISOStr() + ": Slow Button clicked");
	document.getElementById("stopBtn").classList.remove("disabled");
	document.getElementById("slowBtn").classList.add("disabled");
	document.getElementById("normalBtn").classList.remove("disabled");
	document.getElementById("fastBtn").classList.remove("disabled");
	currentSpeed = parseInt(document.getElementById("slowBtn").getAttribute("value"), 10);
	adminSetTrainSpeedPromise(trainId, currentSpeed)
}

function normalBtnClicked() {
	console.log(dtISOStr() + ": Normal Button clicked");
	document.getElementById("stopBtn").classList.remove("disabled");
	document.getElementById("slowBtn").classList.remove("disabled");
	document.getElementById("normalBtn").classList.add("disabled");
	document.getElementById("fastBtn").classList.remove("disabled");
	currentSpeed = parseInt(document.getElementById("normalBtn").getAttribute("value"), 10);
	adminSetTrainSpeedPromise(trainId, currentSpeed)
}

function fastBtnClicked() {
	console.log(dtISOStr() + ": Fast Button clicked");
	document.getElementById("stopBtn").classList.remove("disabled");
	document.getElementById("slowBtn").classList.remove("disabled");
	document.getElementById("normalBtn").classList.remove("disabled");
	document.getElementById("fastBtn").classList.add("disabled");
	currentSpeed = parseInt(document.getElementById("fastBtn").getAttribute("value"), 10);
	adminSetTrainSpeedPromise(trainId, currentSpeed)
}

function disableAllSpeedButtons() {
	document.getElementById("stopBtn").classList.add("disabled");
	document.getElementById("slowBtn").classList.add("disabled");
	document.getElementById("normalBtn").classList.add("disabled");
	document.getElementById("fastBtn").classList.add("disabled");
}

function enableSpeedButtonsBasedOnCurrSpeed() {
	if (currentSpeed == parseInt(document.getElementById("stopBtn").getAttribute("value"), 10)) {
		document.getElementById("slowBtn").classList.remove("disabled");
		document.getElementById("normalBtn").classList.remove("disabled");
		document.getElementById("fastBtn").classList.remove("disabled");
	} else if (currentSpeed == parseInt(document.getElementById("slowBtn").getAttribute("value"), 10)) {
		document.getElementById("stopBtn").classList.remove("disabled");
		document.getElementById("normalBtn").classList.remove("disabled");
		document.getElementById("fastBtn").classList.remove("disabled");
	} else if (currentSpeed == parseInt(document.getElementById("normalBtn").getAttribute("value"), 10)) {
		document.getElementById("stopBtn").classList.remove("disabled");
		document.getElementById("slowBtn").classList.remove("disabled");
		document.getElementById("fastBtn").classList.remove("disabled");
	} else if (currentSpeed == parseInt(document.getElementById("fastBtn").getAttribute("value"), 10)) {
		document.getElementById("stopBtn").classList.remove("disabled");
		document.getElementById("slowBtn").classList.remove("disabled");
		document.getElementById("normalBtn").classList.remove("disabled");
	} else {
		document.getElementById("stopBtn").classList.remove("disabled");
		document.getElementById("slowBtn").classList.remove("disabled");
		document.getElementById("normalBtn").classList.remove("disabled");
		document.getElementById("fastBtn").classList.remove("disabled");
	}
}

function swapDirBtnClicked() {
	console.log((new Date()).toISOString() + ": Swap Direction Button clicked");
	//document.getElementById("swapDirBtn").classList.add("active");
	speedModifier = speedModifier * -1;
	if (currentSpeed > 0) {
		adminSetTrainSpeedPromise(trainId, currentSpeed);
		//	.finally(() => document.getElementById("swapDirBtn").classList.remove("active"));
	} else {
		// Trick to ensure headlights update even when stopped.
		adminSetTrainSpeedPromise(trainId, 1) // arg 1 as speedModifier global takes care of ensuring -1.
			.then(() => wait(150))
			.then(() => adminSetTrainSpeedPromise(trainId, 0));
			//.finally(() => document.getElementById("swapDirBtn").classList.remove("active"));
	}
}

function disableDirectionButton() {
	document.getElementById("swapDirBtn").classList.add("disabled");
}


function enableDirectionButton() {
	document.getElementById("swapDirBtn").classList.remove("disabled");
}

function initialise() {
	// Get train id by getting value of certain label attribute.
	trainId = document.getElementById("trainNameLabel").getAttribute("value");
	console.log("Train ID for this page: " + trainId);
	
	resetMutGlobalVarsToDefault();
	// Grab train if possible, then if grabbed, make sure default direction of page
	// is consistent with direction of train on platform.
	enableSpeedButtonsBasedOnTrainOnTrackPromise();
}

$(document).ready(() => {
	initialise();
});