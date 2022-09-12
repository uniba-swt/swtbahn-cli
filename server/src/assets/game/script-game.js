var driver = null;           // Train driver logic.
var serverAddress = ""; //= "http://141.13.32.44:8080"; // The address of the server.

const numberOfDestinationsMax = 8;
const destinationNamePrefix = "destination";

var allPossibleDestinations = null;

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
	console.log("clearing the interval");
	driver.clearRouteAvailabilityInterval();

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

function setDestinationButton(choice, route) {
	// FIXME: Use signal-specific styles
	const [destinationSignal, routeDetails] = unpackRoute(route);
	
	$(`#${destinationNamePrefix}${choice}`).val(JSON.stringify(route));
	$(`#${destinationNamePrefix}${choice}`).addClass(destinationEnabledButtonStyle);
	$(`#${destinationNamePrefix}${choice}`).removeClass(disabledButtonStyle);
}

function setDestinationButtonAvailable(choice, route) {	
	setDestinationButton(choice, route);
	$(`#${destinationNamePrefix}${choice}`).prop('disabled', false);
}

function setDestinationButtonUnavailable(choice, route) {
	setDestinationButton(choice, route);
	$(`#${destinationNamePrefix}${choice}`).prop('disabled', true);
}

const updatePossibleRoutesTimeout = 500;

function updatePossibleRoutes(blockId) {
	$('#destinationsForm').show();
	
	disableAllDestinationButtons();
	driver.routeAvailabilityInterval = setInterval(() => {
		console.log("Checking available destinations ... ");
		
		const routes = getRoutes(blockId);
		if (routes == null) {
			return;
		}
		
		Object.keys(routes).forEach((destinationSignal, choice) => {
			const route = { };
			route[destinationSignal] = routes[destinationSignal];

			const [_destinationSignal, routeDetails] = unpackRoute(route);
			let routeId = routeDetails["route-id"];
			updateRouteAvailability(
				routeId,
				// route is available
				() => { setDestinationButtonAvailable(choice, route); },
				// route is unavailable
				() => { setDestinationButtonUnavailable(choice, route); }
			);
		});	
	}, updatePossibleRoutesTimeout);
}

function updateRouteAvailability(routeId, available, unavailable) {
	console.log(`Checking availability of route ${routeId}`);
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/route',
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
			if (isAvailable) {
				available();
			} else {
				unavailable();
			}
		},
		error: (responseData, textStatus, errorThrown) => {
			setResponseDanger('#serverResponse', '😢 There was a problem checking the destinations');
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
	$('#speedForm').show();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', false);
	});
}

function disableReachedDestinationButton() {
	$('#destinationReachedForm').hide();
	$('#destinationReached').prop('disabled', true);
}

function enableReachedDestinationButton() {
	$('#destinationReachedForm').show();
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
	currentBlock = null;
	
	trainAvailabilityInterval = null;
	routeAvailabilityInterval = null;
	
	constructor(trackOutput, trainEngine, trainId, userId) {
		this.sessionId = 0;
		this.grabId = -1;

		this.trackOutput = trackOutput;
		this.trainEngine = trainEngine;
		this.trainId = trainId;
		this.userId = userId;

		this.routeDetails = null;
		this.drivingIsForwards = null;
		this.currentBlock = null;
		
		this.trainAvailabilityInterval = null;
		this.routeAvailabilityInterval = null;
	}
	
	get hasValidTrainSession() {
		return (this.sessionId != 0 && this.grabId != -1)
	}
	
	get hasRouteGranted() {
		return (this.routeDetails != null);
	}
	
	clearTrainAvailabilityInterval() {
		clearInterval(this.trainAvailabilityInterval);
	}
	
	clearRouteAvailabilityInterval() {
		clearInterval(this.routeAvailabilityInterval);
	}
	
	updateTrainAvailability() {
		$('.selectTrainButton').prop("disabled", true);
		const trainAvailabilityTimeout = 1000;
		this.trainAvailabilityInterval = setInterval(() => {
			// Enable a train if it is on the platform and has not been grabbed
			$('.selectTrainButton').each((index, obj) => {
				let trainId = obj.id;
				trainIsAvailable(
					trainId, 
					() => {
						if ($(obj).prop("disabled") == true) {
							$(obj).prop("disabled", false)
						}
					},
					() => {
						if ($(obj).prop("disabled") == false) {
							$(obj).prop("disabled", true)
						}
					}
				)
			})
		}, trainAvailabilityTimeout);
	}
	
	grabTrainPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/grab-train',
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
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 There was a problem starting your train');
			}
		});
	}
	
	updateDrivingDirectionPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/monitor/train-state',
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
			url: serverAddress + '/driver/set-dcc-train-speed',
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
			url: serverAddress + '/driver/release-train',
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
			url: serverAddress + '/driver/request-route-id',
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
			url: serverAddress + '/driver/drive-route',
			crossDomain: true,
			data: {
				'session-id': this.sessionId, 
				'grab-id': this.grabId, 
				'route-id': this.routeDetails["route-id"],
				'mode': 'manual'
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
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
			url: serverAddress + '/controller/release-route',
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
		enableSpeedButtons();


		const [destinationSignal, routeDetails] = unpackRoute(route);
		
		this.requestRouteIdPromise(routeDetails)
			.then(() => this.updateDrivingDirectionPromise())
			.then(() => disableAllDestinationButtons())
			.then(() => enableSpeedButtons())
		// FIXME: Only enable the reached destination button 
		// when the train is in the destination segment.
			.then(() => enableReachedDestinationButton())
			.then(() => this.driveRoutePromise())
		// FIXME: .then(() => this.updateCurrentBlock())
		// FIXME: .then(() => updatePossibleRoutes(driver.currentBlock))
			.then(() => updatePossibleRoutes(routeDetails["block"]))   // FIXME: Delete after implementing auto-detection of train's current block
			.then(() => disableSpeedButtons())
			.then(() => disableReachedDestinationButton());
	}
}

function trainIsAvailable(trainId, success, error) {
	$.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/train-state',
		crossDomain: true,
		data: { 'train': trainId },
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			if(responseData.includes("grabbed: no") && !responseData.includes("on segment: no")){
				success();
			} else {
				error();
			}
		}
	});
}

// Start the game logic
function startGameLogic() {
	// FIXME: On iOS, speech synthesis only works if it is first triggered by the user.
	speak("");

	// FIXME: Delete after implementing auto-detection of train's current block
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
	// FIXME: .then(() => driver.updateCurrentBlock())
		.then(() => updatePossibleRoutes(driver.currentBlock))
		.always(() => driver.clearTrainAvailabilityInterval());
}

function initialise() {
	// Hide the train driving buttons (destinations and speed selections)
	$('#endGameButton').hide();	
	$('#destinationsForm').hide();
	disableReachedDestinationButton();
	disableSpeedButtons();

	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	driver = new Driver(
		'master',                                 // trackOutput
		'libtrain_engine_default (unremovable)',  // trainEngine
		null,		                              // trainId
		null                                      // userId
	);
	
	// Update all train selections
	driver.updateTrainAvailability();

	// Handle train selection
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id;
		driver.trainId = trainId;
		startGameLogic();
	});
	
	// Set the possible destinations for the SWTbahn platform.
//	allPossibleDestinations = allPossibleDestinationsSwtbahnFull;
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
	
	disableReachedDestinationButton();
	
	// Initialise the click handler of the destination reached button.
	$("#destinationReached").click(function () {
		driver.setTrainSpeedPromise(0);
		driver.releaseRoutePromise();
	});
	
	disableSpeedButtons();
	
	// Initialise the click handler of each speed button.
	speedButtons.forEach(speed => {
		const speedButton = $(`#${speed}`);
		speedButton.click(function () {
			driver.setTrainSpeedPromise(speedButton.val());
		});
	});
}

// Wait for a duration in milliseconds.
function wait(duration) { 
	return new Promise(resolve => setTimeout(resolve, duration));
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

		$('#endGameButton').click(function () {
			if (!driver.hasValidTrainSession) {
				setResponseSuccess('#serverResponse', '😀 Thank you for playing');
				$('#trainSelection').show();
				$('#endGameButton').hide();
				$('#destinationsForm').hide();
				disableReachedDestinationButton();
				disableSpeedButtons();
				driver.updateTrainAvailability();

				driver.trainId = null;
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
					disableReachedDestinationButton();
					disableSpeedButtons();
					driver.updateTrainAvailability();
				});
		});
	}
);
