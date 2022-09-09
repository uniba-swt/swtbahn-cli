var driver = null;           // Train driver logic.
// var gameBlockId = null;      // Initial railway block for the game.
var serverAddress = "http://141.13.32.44:8080"; // The address of the server.

const numberOfDestinationsMax = 8;
const destinationNamePrefix = "destination";

var allPossibleDestinations = null;

const allPossibleDestinationsSwtbahnUltraloop = {
	'block0': {                     // Source block
		
		// Route
		"signal4": {                    // Destination signal of route
			"route-id": 0,                  // Required route ID
			"orientation": "clockwise",     // Route orientation
			"block": "block1",              // Block ID of destination signal
			"segment": "seg29"              // ID of main segment of destination block
		},
		
		// Route
		"signal6": {                    // Destination signal of route
			"route-id": 1,                  // Required route ID
			"orientation": "anticlockwise", // Route orientation
			"block": "block1",              // Block ID of destination signal
			"segment": "seg17"              // ID of main segment of destination block
		}
	}
};

const allPossibleDestinationsSwtbahnStandard = {
	'buffer': {
		"signal6": {
			"route-id": 57,
			"orientation": "anticlockwise",
			"block": "block2",
			"segment": "seg8"
		}
	},
	'block1': {
		"signal1": {
			"route-id": 44,
			"orientation": "clockwise",
			"block": "buffer",
			"segment": "seg1"
		},
		"signal7": {
			"route-id": 47,
			"orientation": "clockwise",
			"block": "block3",
			"segment": "seg12"
		},
		"signal6": {
			"route-id": 57,
			"orientation": "anticlockwise",
			"block": "block2",
			"segment": "seg8"
		}
	},
	'block2': {
		"signal2": {
			"route-id": 77,
			"orientation": "clockwise",
			"block": "block1",
			"segment": "seg4"
		},
		"signal9": {
			"route-id": 75,
			"orientation": "clockwise",
			"block": "block5",
			"segment": "seg28"
		},
		"signal10": {
			"route-id": 78,
			"orientation": "clockwise",
			"block": "block4",
			"segment": "seg16"
		},
		"signal8": {
			"route-id": 104,
			"orientation": "anticlockwise",
			"block": "block3",
			"segment": "seg12"
		},
		"signal11": {
			"route-id": 111,
			"orientation": "anticlockwise",
			"block": "block5",
			"segment": "seg28"
		},
		"signal14": {
			"route-id": 114,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		},
		"signal15": {
			"route-id": 108,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		}
	},
	'block3': {
		"signal4": {
			"route-id": 120,
			"orientation": "clockwise",
			"block": "block2",
			"segment": "seg8"
		},
		"signal3": {
			"route-id": 6,
			"orientation": "anticlockwise",
			"block": "block1",
			"segment": "seg4"
		},
		"signal12": {
			"route-id": 23,
			"orientation": "anticlockwise",
			"block": "block4",
			"segment": "seg16"
		}
	},
	'block4': {
		"signal7": {
			"route-id": 70,
			"orientation": "clockwise",
			"block": "block3",
			"segment": "seg12"
		},
		"signal6": {
			"route-id": 221,
			"orientation": "anticlockwise",
			"block": "block2",
			"segment": "seg8"
		}
	},
	'block5': {
		"signal4": {
			"route-id": 33,
			"orientation": "clockwise",
			"block": "block2",
			"segment": "seg8"
		},
		"signal13": {
			"route-id": 40,
			"orientation": "clockwise",
			"block": "block6",
			"segment": "seg21"
		},
		"signal17": {
			"route-id": 39,
			"orientation": "clockwise",
			"block": "platform1",
			"segment": "seg37"
		},
		"signal19": {
			"route-id": 26,
			"orientation": "clockwise",
			"block": "platform2",
			"segment": "seg40"
		},		
		"signal5": {
			"route-id": 203,
			"orientation": "anticlockwise",
			"block": "block6",
			"segment": "seg21"
		},
		"signal6": {
			"route-id": 204,
			"orientation": "anticlockwise",
			"block": "block2",
			"segment": "seg8"
		}
	},
	'block6': {
		"signal9": {
			"route-id": 229,
			"orientation": "clockwise",
			"block": "block5",
			"segment": "seg28"
		},
		"signal11": {
			"route-id": 96,
			"orientation": "anticlockwise",
			"block": "block5",
			"segment": "seg28"
		},
		"signal14": {
			"route-id": 100,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		},
		"signal15": {
			"route-id": 94,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		}
	},
	'block7': {
		"signal17": {
			"route-id": 260,
			"orientation": "clockwise",
			"block": "platform1",
			"segment": "seg37"
		},
		"signal19": {
			"route-id": 243,
			"orientation": "clockwise",
			"block": "platform2",
			"segment": "seg40"
		},
		"signal4": {
			"route-id": 255,
			"orientation": "clockwise",
			"block": "block2",
			"segment": "seg8"
		},
		"signal13": {
			"route-id": 261,
			"orientation": "clockwise",
			"block": "block6",
			"segment": "seg21"
		}
	},
	'platform1': {
		"signal11": {
			"route-id": 152,
			"orientation": "anticlockwise",
			"block": "block5",
			"segment": "seg28"
		},
		"signal15": {
			"route-id": 151,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		}
	},
	'platform2': {
		"signal11": {
			"route-id": 187,
			"orientation": "anticlockwise",
			"block": "block5",
			"segment": "seg28"
		},
		"signal14": {
			"route-id": 193,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		},
		"signal15": {
			"route-id": 185,
			"orientation": "anticlockwise",
			"block": "block7",
			"segment": "seg31"
		}
	}
};

const allPossibleDestinationsSwtbahnFull = {
	// FIXME: To be filled in.
};

function getRoutes(blockId) {
	if (allPossibleDestinations == null || !allPossibleDestinations.hasOwnProperty(blockId)) {
		return null;
	}
	
	return allPossibleDestinations[blockId];
}

function unpackRoute(route) {
	for (let destinationSignal in route) {
    	return [destinationSignal, route[destinationSignal]];
	}
	return null;
}

const disabledButtonStyle = 'btn-outline-secondary';
const destinationEnabledButtonStyle = 'btn-dark';
const destinationHighlightedButtonStyle = 'btn-dark';

function disableAllDestinationButtons() {
	console.log("clearing the interval")
	clearInterval(driver.routeAvailabilityInterval)

	// FIXME: Remove the signal-specific CSS styles
	for (let i = 0; i < numberOfDestinationsMax; i++) {
		$(`#${destinationNamePrefix}${i}`).val("");
		$(`#${destinationNamePrefix}${i}`).prop('disabled', true);
		$(`#${destinationNamePrefix}${i}`).removeClass(destinationEnabledButtonStyle);
		$(`#${destinationNamePrefix}${i}`).removeClass(destinationHighlightedButtonStyle);
		$(`#${destinationNamePrefix}${i}`).addClass(disabledButtonStyle);
	}
}

function highlightDestinationButton(choice, route) {
	// FIXME: Use signal-specific styles
	const [destinationSignal, routeDetails] = unpackRoute(route);
	
	$(`#${destinationNamePrefix}${choice}`).addClass(destinationHighlightedButtonStyle);
}

function setDestinationButtonAvailable(choice, route) {
	// FIXME: Use signal-specific styles
	const [destinationSignal, routeDetails] = unpackRoute(route);
	
	const routeString = JSON.stringify(route);

	$(`#${destinationNamePrefix}${choice}`).val(routeString);
	$(`#${destinationNamePrefix}${choice}`).prop('disabled', false);
	$(`#${destinationNamePrefix}${choice}`).addClass(destinationEnabledButtonStyle);
	$(`#${destinationNamePrefix}${choice}`).removeClass(disabledButtonStyle);
}

function setDestinationButtonUnavailable(choice, route) {
	// FIXME: Use signal-specific styles
	const [destinationSignal, routeDetails] = unpackRoute(route);
	
	const routeString = JSON.stringify(route);

	$(`#${destinationNamePrefix}${choice}`).val(routeString);
	$(`#${destinationNamePrefix}${choice}`).prop('disabled', true);
	$(`#${destinationNamePrefix}${choice}`).addClass(destinationEnabledButtonStyle);
	$(`#${destinationNamePrefix}${choice}`).removeClass(disabledButtonStyle);
}

function updatePossibleRoutes(blockId) {
	$('#destinationsForm').show();
	// $('#destinationReachedForm').show();
	// $('#speedForm').show();
	
	disableAllDestinationButtons();
	driver.routeAvailabilityInterval = setInterval(() => {
		console.log("checking available routes .. ")
		
		const routes = getRoutes(blockId);
		if (routes == null) {
			return;
		}
		
		Object.keys(routes).forEach((destinationSignal, choice) => {
			const route = { };
			route[destinationSignal] = routes[destinationSignal];
			console.log(`Route ${choice}: ${JSON.stringify(route)}`);

			const [_destinationSignal, routeDetails] = unpackRoute(route);
			let routeId = routeDetails["route-id"]
			updateRouteAvailability(
				routeId,
				// route is available
				() => { setDestinationButtonAvailable(choice, route); },
				// route is unavailable
				() => { setDestinationButtonUnavailable(choice, route); }
			)
		});	
}, 500)
}

function updateRouteAvailability(routeId, available, unavailable) {
	console.log(`Checking availability of route ${routeId}`);
	return $.ajax({
		type: 'POST',
		url: serverAddress+'/monitor/route',
		crossDomain: true,
		data: {
			'route-id': routeId,
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const isNotConflicting = responseData.includes("granted conflicting route ids: none");
			const isRouteClear = responseData.includes("route clear: yes");
			const isNotGranted = responseData.includes("granted train: none");

			const isAvailable = isNotConflicting && isRouteClear && isNotGranted;
			if(isAvailable) {
				available()
			} else {
				unavailable()
			}
		},
		error: (responseData, textStatus, errorThrown) => {
			// can be ignored
			// setResponseDanger('#serverResponse', '😢 There was a problem starting your train');
		}
	});
	
}

const speedButtons = [
	"stop",
	"slow",
	"normal",
	"fast"
];

function disableSpeedButtons() {
	$('#speedForm').hide();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', true);
	});
}

function enableSpeedButtons() {
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', false);
	});
}

function disableReachedDestinationButton() {
	$('#destinationReachedForm').hide();
	$('#destinationReached').prop('disabled', true);
}

function enableReachedDestinationButton() {
	$('#destinationReached').prop('disabled', false);
}

var responseTimer = null;
const responseTimeout = 7000;

function setResponse(responseId, message, callback) {
	clearTimeout(responseTimer);
	
	$(responseId).text(message);
	callback();
	
	$(responseId).parent().fadeIn("fast");
	responseTimer = setTimeout(() => {
		$(responseId).parent().fadeOut("slow");
	}, responseTimeout);
}

function speak(text) {
    var msg = new SpeechSynthesisUtterance(text);
    for (const voice of window.speechSynthesis.getVoices()) {
        if (voice.lang == "de-DE") {
            msg.voice = voice;
            break;
        }
    }
    window.speechSynthesis.speak(msg);
}

function setResponseDanger(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().addClass('alert-danger-blink');
		$(responseId).parent().removeClass('alert-success');
	});
	
	// FIXME: Replace with better game sounds.
	speak('NEIN, NEIN, NEIN!');
}

function setResponseSuccess(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});
}

class Driver {
	sessionId = null;
	grabId = null;
	userId = null;

	trackOutput = null;
	trainEngine = null;
	trainId = null;

	routeDetails = null;
	drivingIsForwards = null;
	
	constructor(trackOutput, trainEngine, trainId, userId) {
		this.sessionId = 0;
		this.grabId = -1;

		this.trackOutput = trackOutput;
		this.trainEngine = trainEngine;
		this.trainId = trainId;
		this.userId = userId;

		this.routeDetails = null;
		this.drivingIsForwards = null;
	}
	
	get hasValidTrainSession() {
		return (this.sessionId != 0 && this.grabId != -1)
	}
	
	get hasRouteGranted() {
		return (this.routeDetails != null);
	}
	
	grabTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/driver/grab-train',
			crossDomain: true,
			data: {
				'train': this.trainId,
				'engine': this.trainEngine
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				const responseDataSplit = responseData.split(',');
				this.sessionId = responseDataSplit[0];
				this.grabId = responseDataSplit[1];
			
				setResponseSuccess('#serverResponse', '😁 Your train is ready');
				$('#trainSelection').hide();
				$('#endGameButton').show();
				$('#trainDetails').html(driver.trainId);
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 There was a problem starting your train');
			}
		});
	}
	
	updateDrivingDirectionPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/monitor/train-state',
			crossDomain: true,
			data: {
				'train': this.trainId
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				const trainIsLeft = responseData.includes('left');
				const routeIsClockwise = (this.routeDetails["orientation"] == "clockwise");
				this.drivingIsForwards = (routeIsClockwise && trainIsLeft)
				                         || (!routeIsClockwise && !trainIsLeft);
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 Could not find your train');
			}
		});
	}

	setTrainSpeedPromise(speed) {
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/driver/set-dcc-train-speed',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'speed': this.drivingIsForwards ? speed : "-" + speed,
				'track-output': this.trackOutput
			},
			dataType: 'text',
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 There was a problem setting the speed of your train');
			}
		});
	}

	releaseTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/driver/release-train',
			crossDomain: true,
			data: {
				'session-id': this.sessionId, 
				'grab-id': this.grabId
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.sessionId = 0;
				this.grabId = -1;
			
				setResponseSuccess('#serverResponse', '😀 Thank you for playing');
				$('#endGameButton').hide();
				$('#trainSelection').show();
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '🤔 There was a problem ending your turn');
			}
		});
	}
	
	requestRouteIdPromise(routeDetails) {
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/driver/request-route-id',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'route-id': routeDetails["route-id"]
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.routeDetails = routeDetails;
				setResponseSuccess('#serverResponse', '🥳 Start driving your train to your chosen destination');
			},
			error: (responseData, textStatus, errorThrown) => {
				this.routeDetails = null;
				setResponseDanger('#serverResponse', responseData.responseText);
			}
		});
	}

	driveRoutePromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/driver/drive-route',
			crossDomain: true,
			data: {
				'session-id': this.sessionId, 
				'grab-id': this.grabId, 
				'route-id': this.routeDetails["route-id"],
				'mode': 'manual'
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.currentBlock = this.routeDetails["block"]
				this.routeDetails = null;
				setResponseSuccess('#serverResponse', '🥳 You drove your train to your chosen destination');
			},
			error: (responseData, textStatus, errorThrown) => {
				this.routeDetails = null;
				setResponseDanger('#serverResponse', '😢 Route to your chosen destination is unavailable');
			}
		});
	}

	releaseRoutePromise() {
		if (!this.hasRouteGranted) {
			return;
		}
		
		return $.ajax({
			type: 'POST',
			url: serverAddress+'/controller/release-route',
			crossDomain: true,
			data: { 'route-id': this.routeDetails["route-id"] },
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.routeDetails = null;
			}
		});
	}

	driveToPromise(route) {
		if (!this.hasValidTrainSession) {
			setResponseDanger('#serverResponse', 'Your train could not be found 😢');
			return;
		}

		$('#destinationsForm').hide();
		$('#destinationReachedForm').show();
		$('#speedForm').show();


		const [destinationSignal, routeDetails] = unpackRoute(route);
		
		this.requestRouteIdPromise(routeDetails)
			.then(() => this.updateDrivingDirectionPromise())
			.then(() => disableAllDestinationButtons())
			.then(() => enableSpeedButtons())
			.then(() => this.driveRoutePromise())
			.then(() => updatePossibleRoutes(routeDetails["block"]))
			.then(() => disableSpeedButtons())
			.then(() => disableReachedDestinationButton());
		
		// FIXME: Only enable the reached destination button 
		// when the train is in the destination segment.
		enableReachedDestinationButton();
	}
}

function trainIsAvailable(trainId, success, error){
	$.ajax({
		type: 'POST',
		url: serverAddress+'/monitor/train-state',
		crossDomain: true,
		data: { 'train': trainId },
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			if(responseData.includes("grabbed: no") && !responseData.includes("on segment: no")){
				success();
			} else {
				error();
			}
		},
		error: (responseData, textStatus, errorThrown) => {
			// ignore for now .. 
		}
	});
}

function initialise() {

	$('#destinationsForm').hide();
	$('#destinationReachedForm').hide();
	$('#speedForm').hide();

	driver = new Driver(
		'master',                                 // trackOutput
		'libtrain_engine_default (unremovable)',  // trainEngine
		null,		                               // trainId
		null                                      // userId
	);
	
	// gameBlockId = 'platform1';
	// driver.currentBlock =  gameBlockId;
	driver.routeAvailabilityInterval = null;

	// set all trains to disabled
	$('.selectTrainButton').prop("disabled", true);
	driver.trainAvailabilityInterval = setInterval(() => {
		// enable a train if it is on the platform and has not been grabbed so far
		$('.selectTrainButton').each((index, obj) => {
			let trainId = obj.id
			trainIsAvailable(
				trainId, 
				()=>	{
					if($(obj).prop("disabled") == true){
						$(obj).prop("disabled", false)
					}
				},
				()=>	{
					if($(obj).prop("disabled") == false){
						$(obj).prop("disabled", true)
					}
				}
			)
		})
	}, 1000);

	// Display the train name and user name.
	// $('#trainDetails').html(driver.trainId);

	// Handle train selection
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id
		driver.trainId = trainId;

		// $('#trainDetails').html(driver.trainId);
		startGameLogic()
	});
	
	// FIXME: Quick way to test other players.
	// $('#trainDetails').click(function () {
		// Set the source signal for the train's starting position.
		// driver.trainId = 'cargo_green';
		// gameBlockId = 'platform2';
		
		// $('#trainDetails').html(driver.trainId);
	// });
	
	// Show only the start game button.
	// $('#startGameButton').show();
	$('#endGameButton').hide();
	
	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations = allPossibleDestinationsSwtbahnStandard;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnUltraloop;
	disableAllDestinationButtons();
	
	// Initialise the click handler of each destination button.
	for (let i = 0; i < numberOfDestinationsMax; i++) {
		const destinationButton = $(`#${destinationNamePrefix}${i}`);
		destinationButton.click(function () {
			const route = JSON.parse(destinationButton.val());
			driver.driveToPromise(route);
		});
	}
	
	disableSpeedButtons();
	
	// Initialise the click handler of each speed button.
	speedButtons.forEach(speed => {
		const speedButton = $(`#${speed}`);
		speedButton.click(function () {
			driver.setTrainSpeedPromise(speedButton.val());
		});
	});
	
	disableReachedDestinationButton();
	
	// Initialise the click handler of the destination reached button.
	$("#destinationReached").click(function () {
		driver.setTrainSpeedPromise(0);
		driver.releaseRoutePromise();
	});
}

// Wait for a duration in milliseconds.
function wait(duration) { 
	return new Promise(resolve => setTimeout(resolve, duration));
}

// Start the game logic
let startGameLogic = function () {
	// FIXME: On iOS, speech synthesis only works if it is first triggered by the user.
	speak("");


	if(driver.trainId == 'cargo_db') {
		driver.currentBlock = 'platform1';
	} else if(driver.trainId == 'cargo_green') {
		driver.currentBlock = 'platform2';
	} else if(driver.trainId == 'regional_odeg') {
		driver.currentBlock = 'block1';
	}

	if (driver.hasValidTrainSession) {
		setResponseDanger('#serverResponse', 'You are already driving a train!')
		return;
	}
	
	setResponseSuccess('#serverResponse', '⏳ Waiting ...');
				
	driver.grabTrainPromise()
		.then(() => updatePossibleRoutes(driver.currentBlock))
		.always(() => 			clearInterval(driver.trainAvailabilityInterval));
}

$(document).ready(
	function () {
		//-----------------------------------------------------
		// Initialisation
		//-----------------------------------------------------
		initialise();


		//-----------------------------------------------------
		// Button behaviours
		//-----------------------------------------------------

		// $('#startGameButton').click(function () {
		// 	startGameLogic()
		// });

		$('#endGameButton').click(function () {
			if (!driver.hasValidTrainSession) {
				setResponseSuccess('#serverResponse', '😀 Thank you for playing');
				// $('#startGameButton').show();
				$('#endGameButton').hide();
				$('#trainSelection').show();
				$('#destinationsForm').hide();
				$('#destinationReachedForm').hide();
				$('#speedForm').hide();

				driver.trainId = null;
				$('#trainDetails').html(driver.trainId);
				return;
			}
			
			setResponseSuccess('#serverResponse', '⏳ Waiting ...');
			
			driver.blockId = null;
			
			driver.setTrainSpeedPromise(0)
			.then(() => wait(500))
			.then(() => driver.releaseRoutePromise())
			.always(() => {
				driver.releaseTrainPromise();
				disableAllDestinationButtons();
				$('#destinationsForm').hide();
				$('#destinationReachedForm').hide();
				$('#speedForm').hide();
				});
		});

	}	
);



