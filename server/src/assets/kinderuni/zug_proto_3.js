var trainGrabbed = false;
var speedModifier = 1; // forward = 1, backward = -1
var sessionId = 0;
var grabId = -1;
var currentSpeed = 0;
var trainId = "cargo_db";

const serverAddress = "";
const trainEngine = "libtrain_engine_default (unremovable)";
const trackOutput = "master";

function resetMutGlobalVarsToDefault() {
	trainGrabbed = false;
	speedModifier = 1;
	sessionId = 0;
	grabId = -1;
	currentSpeed = 0;
}

function dtISOStr() {
	return (new Date()).toISOString();
}

// Asynchronous wait for a duration in milliseconds.
function wait(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
}

function setTrainSpeedPromise(speed) {
	console.log(dtISOStr() + ": setTrainSpeedPromise(" + speed + ")");
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/driver/set-dcc-train-speed',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId,
			'speed': speedModifier * speed,
			'track-output': trackOutput
		},
		dataType: 'text',
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": setTrainSpeedPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function grabTrainPromise() {
	console.log(dtISOStr() + ": grabTrainPromise() - train " + trainId);
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/driver/grab-train',
		crossDomain: true,
		data: {
			'train': trainId,
			'engine': trainEngine
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const responseDataSplit = responseData.split(',');
			sessionId = responseDataSplit[0];
			grabId = responseDataSplit[1];
			trainGrabbed = true;
			currentSpeed = 0;
			enableSpeedButtonsBasedOnCurrSpeed();
			enableDirectionButton();
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": grabTrainPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function releaseTrainPromise() {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/driver/release-train',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			sessionId = 0;
			grabId = -1;
			trainGrabbed = false;
			// when releasing, the server will set the train's speed to 0.
			currentSpeed = 0;
			disableAllSpeedButtons();
			enableSpeedButtonsBasedOnCurrSpeed();
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": releaseTrainPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function forceReleaseTrainPromise() {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/admin/release-train',
		crossDomain: true,
		data: {
			'train': trainId
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			sessionId = 0;
			grabId = -1;
			trainGrabbed = false;
			// when releasing, the server will set the train's speed to 0.
			currentSpeed = 0;
			disableAllSpeedButtons();
			disableDirectionButton();
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": forceReleaseTrainPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function getReportedTrainSpeedPromise() {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/train-state',
		crossDomain: true,
		data: {
			'train': this.trainId
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const regexMatch = /speed step: (.*?) /g.exec(responseData);
			const reportedSpeed = regexMatch[1];
			return reportedSpeed;
		},
		error: (responseData, textStatus, errorThrown) => {
			return -1;
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
	// When stopping, it is especially important that the train is actually stopping.
	// Therefore, here we integrate an experimental check to see how fast the train is
	// a bit later, and to stop it again if need be.
	setTrainSpeedPromise(currentSpeed)
		.then(() => wait(200))
		.then(() => getReportedTrainSpeedPromise())
		.then(repTrSpeed => {
			console.log(dtISOStr() + ": Reported Train Speed: " + repTrSpeed);
			if (repTrSpeed !== 0 && currentSpeed === 0) {
				console.log(dtISOStr() + ": StopBtnClicked - train not at speed 0 after wait period -> stop again");
				setTrainSpeedPromise(currentSpeed);
			} else {
				console.log(dtISOStr() + ": StopBtnClicked - train is at desired speed 0.");
			}
		}).catch(() => {
			console.log(dtISOStr() + ": StopBtnClicked - Setting train speed was not successful!");
			// Now try and reverse effect of clicked stop button
			currentSpeed = oldspeed;
			enableSpeedButtonsBasedOnCurrSpeed();
		});
}

function slowBtnClicked() {
	console.log(dtISOStr() + ": Slow Button clicked");
	document.getElementById("stopBtn").classList.remove("disabled");
	document.getElementById("slowBtn").classList.add("disabled");
	document.getElementById("normalBtn").classList.remove("disabled");
	document.getElementById("fastBtn").classList.remove("disabled");
	currentSpeed = parseInt(document.getElementById("slowBtn").getAttribute("value"), 10);
	setTrainSpeedPromise(currentSpeed);
}

function normalBtnClicked() {
	console.log(dtISOStr() + ": Normal Button clicked");
	document.getElementById("stopBtn").classList.remove("disabled");
	document.getElementById("slowBtn").classList.remove("disabled");
	document.getElementById("normalBtn").classList.add("disabled");
	document.getElementById("fastBtn").classList.remove("disabled");
	currentSpeed = parseInt(document.getElementById("normalBtn").getAttribute("value"), 10);
	setTrainSpeedPromise(currentSpeed);
}

function fastBtnClicked() {
	console.log(dtISOStr() + ": Fast Button clicked");
	document.getElementById("stopBtn").classList.remove("disabled");
	document.getElementById("slowBtn").classList.remove("disabled");
	document.getElementById("normalBtn").classList.remove("disabled");
	document.getElementById("fastBtn").classList.add("disabled");
	currentSpeed = parseInt(document.getElementById("fastBtn").getAttribute("value"), 10);
	setTrainSpeedPromise(currentSpeed);
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
	speedModifier = speedModifier * -1;
	if (currentSpeed > 0) {
		setTrainSpeedPromise(currentSpeed);
	} else {
		// Trick to ensure headlights update even when stopped.
		setTrainSpeedPromise(1) // arg 1 as speedModifier global takes care of ensuring -1.
			.then(() => wait(500))
			.then(() => setTrainSpeedPromise(0));
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
	grabTrainPromise()
		.then(() => {
			console.log(dtISOStr() + ": Initialise -> grabTrainPromise -> then");
			if (trainGrabbed ) {
				enableDirectionButton();
			}
		})
		.catch(() => {
			console.log(dtISOStr() + ": Initialise -> grabTrainPromise -> catch");
			// -> disable all speedbuttons.
			disableAllSpeedButtons();
			disableDirectionButton();
		});
}

$(document).ready(() => {
	initialise();
	wait(500).then(() => {
		if (!trainGrabbed) {
			console.log(dtISOStr() + ": Initialisation failed, train not grabbed. Force release and then try again.");
			// Maybe train could not be grabbed -> Force release and try once more.
			forceReleaseTrainPromise()
				.then(() => initialise())
				.then(() => console.log(dtISOStr() + ": Init worked on 2nd attempt"))
				.catch(() => {
					// Admin release did not work, stop trying to initialise.
					// Todo: maybe display error.
					console.log(dtISOStr() + ": Init failed on 2nd attempt");
				});
		}
	});
});

$(window).on("unload", (event) => {
	//console.log("Unloading");
	// Server Stops train on release automatically.
	if (trainGrabbed) {
		const formData = new FormData();
		formData.append('session-id', sessionId);
		formData.append('grab-id', grabId);
		navigator.sendBeacon(serverAddress + '/driver/release-train', formData);
	}
});