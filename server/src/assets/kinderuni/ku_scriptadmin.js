const serverAddress = "";
const trainEngine = "libtrain_engine_default (unremovable)";
const trackOutput = "master";

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
			// TODO: Trigger Status Display Update 
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": adminReleaseTrainPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function startupPromise() {
	return $.ajax({
		type: 'POST',
		url: '/admin/startup',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			;
		},
		error: function (responseData, textStatus, errorThrown) {
			alert("Startup Failed");
		}
	});
}

function shutdownPromise() {
	return $.ajax({
		type: 'POST',
		url: '/admin/startup',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			;
		},
		error: function (responseData, textStatus, errorThrown) {
			alert("Shutdown Failed");
		}
	});
}

function adjustValueField(valueFieldSelector, value) {
	$(valueFieldSelector).text(value);
}

function updateTrainStatePromise(train) {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/train-state',
		crossDomain: true,
		data: {
			'train': this.trainId
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			// = /grabbed: (.*?) - on segment: (.*?) - on block: (.*?) - orientation: (.*?) - speed step: (.*?) - detected speed (.*?) km/h - direction: (.*?)/
			const regexMatches = /grabbed: (.*?) - on segment: (.*?) - on block: (.*?) - orientation: (.*?) - speed step: (.*?) - detected speed (.*?) km\/h - direction: (.*?) /g.exec(responseData);
			const grabbed = regexMatches[1];
			const segment = regexMatches[2];
			const block = regexMatches[3];
			const orientation = regexMatches[4];
			const speedStep = regexMatches[5];
			const detSpeed = regexMatches[6];
			const direction = regexMatches[7];
			console.log(`Extracted: grabbed ${grabbed}, block ${block}, speed step ${speedStep}, detected Speed ${detSpeed}, orientation ${orientation}`);
			$('#' + train + "_grabbed_value").text(grabbed);
			$('#' + train + "_on_block_value").text(block);
			$('#' + train + "_speed_step_value").text(speedStep);
			$('#' + train + "_detected_speed_value").text(detSpeed);
			$('#' + train + "_orientation_value").text(orientation);
		},
		error: (responseData, textStatus, errorThrown) => {
			;
		}
	});
}

function updateTrainsStates() {
	trains = ["cargo_db", "cargo_bayern"];
	for(train in trains) {
		
	}
}

$(document).ready(
	async function () {
		await new Promise(r => setTimeout(r, 350));
		updateParamAspectsPromise(true)
		.then(() => updatePointsVisuals());
		updateParamAspectsPromise(false)
		.then(() => updateSignalsVisuals());
		
		let updateInterval = setInterval(function() {
			updateParamAspectsPromise(true)
					.then(() => updatePointsVisuals());
			updateParamAspectsPromise(false)
					.then(() => updateSignalsVisuals());
		}, 5000);/**/
	}
);