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
			if (parseInt(speedStep, 10) > 0) {
				$('#' + train + "stop_btn").removeClass("disabled");
			} else {
				$('#' + train + "stop_btn").addClass("disabled");
			}
			$('#monitoring_status_field').text("Active");
		},
		error: (responseData, textStatus, errorThrown) => {
			const current_text = $('#monitoring_status_field').text();
			if (current_text !== "Starting..." && current_text !== "Stopped" && current_text !== "Shutting down...") {
				$('#monitoring_status_field').text("Error");
			}
		}
	});
}

function updateTrainStates() {
	//const trains = ["cargo_db", "cargo_bayern", "cargo_green", "regional_odeg", "regional_brengdirect"];
	const trains = ["cargo_db"];
	for(var tr of trains) {
		updateTrainStatePromise(tr);
	}
}

$(document).ready(
	async function () {
		await new Promise(r => setTimeout(r, 100));
		updateTrainStates();
		
		let updateInterval = setInterval(function() {
			updateTrainStates();
		}, 5000);
	}
);