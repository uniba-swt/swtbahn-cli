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
		console.log("Checking available destinations ...");
		
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
				() => setDestinationButtonAvailable(choice, route),
				// route is unavailable
				() => setDestinationButtonUnavailable(choice, route)
			);
		});	
	}, updatePossibleRoutesTimeout);
}

function updateRouteAvailability(routeId, available, unavailable) {
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
	$('#destination').html("");
	
	$('#speedForm').hide();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', true);
	});
}

function enableSpeedButtons(destination) {
	$('#destination').html(destination);
	
	$('#speedForm').show();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', false);
	});
}

function disableReachedDestinationButton() {
	$('#destinationReachedForm').hide();
	$('#destinationReached').prop('disabled', true);
	driver.endGameButtonIsPersistent = false;
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
	trainId = null;

	trackOutput = null;
	trainEngine = null;
	trainId = null;

	routeDetails = null;
	drivingIsForwards = null;
	currentBlock = null;
	endGameButtonIsPersistent = null;
	
	trainAvailabilityInterval = null;
	routeAvailabilityInterval = null;
	destinationReachedInterval = null;
	
	constructor(trackOutput, trainEngine, trainId) {
		this.sessionId = 0;
		this.grabId = -1;

		this.trackOutput = trackOutput;
		this.trainEngine = trainEngine;
		this.trainId = trainId;

		this.routeDetails = null;
		this.drivingIsForwards = null;
		this.currentBlock = null;
		this.endGameButtonIsPersistent = false;
		
		this.trainAvailabilityInterval = null;
		this.routeAvailabilityInterval = null;
		this.destinationReachedInterval = null;
	}
	
	reset() {
		this.sessionId = 0;
		this.grabId = -1;
	}
	
	get hasValidTrainSession() {
		return (this.sessionId != 0 && this.grabId != -1)
	}
	
	get hasRouteGranted() {
		return (this.routeDetails != null);
	}
	
	clearTrainAvailabilityInterval() {
		console.log("clearTrainAvailabilityInterval");
		clearInterval(this.trainAvailabilityInterval);
	}
	
	clearRouteAvailabilityInterval() {
		console.log("clearRouteAvailabilityInterval");
		clearInterval(this.routeAvailabilityInterval);
	}
	
	clearDestinationReachedInterval() {
		console.log("clearDestinationReachedInterval");
		clearInterval(this.destinationReachedInterval);
	}
	
	updateCurrentBlock() {
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
				this.currentBlock =  regexMatch[1];
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 Could not find your train');
			}
		});
	}
	
	updateTrainAvailability() {
		$('.selectTrainButton').prop("disabled", true);
		const trainAvailabilityTimeout = 1000;
		this.trainAvailabilityInterval = setInterval(() => {
			console.log("Checking available trains ... ");

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
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 There was a problem starting your train');
			}
		});
	}
	
	updateDrivingDirectionPromise() {
		return $.ajax({
			type: 'POST',
			url: serverAddress + '/driver/direction',
			crossDomain: true,
			data: {
				'train': this.trainId,
				'route-id': this.routeDetails["route-id"]
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.drivingIsForwards = responseData.includes("forwards");
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

	enableReachedDestinationButton() {
		const destinationReachedTimeout = 500; 
		this.destinationReachedInterval = setInterval(() => {
			return $.ajax({
				type: 'POST',
				url: serverAddress + '/monitor/train-state',
				crossDomain: true,
				data: {
					'train': this.trainId
				},
				dataType: 'text',
				success: (responseData, textStatus, jqXHR) => {
					const matches = /on segment: (.*?) -/g.exec(responseData); // Get all Segment IDS as String
					const segmentIDs = matches[1];
					const segments = segmentIDs.split(", "); // Splits them into Array
					
					// Note: True when first segment object (it should be one) is the destination last segment
					if (segments.length == 1 && segments[0].includes(this.routeDetails["segment"])) {
						this.clearDestinationReachedInterval();
						this.endGameButtonIsPersistent = true;
						$('#endGameButton').show();
						$(window).unbind("beforeunload", pageRefreshWarning);
						$('#destinationReachedForm').show();
						$('#destinationReached').prop('disabled', false);
					}
				}
			});
		}, destinationReachedTimeout);
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
				this.trainId = null;
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

		const [destinationSignal, routeDetails] = unpackRoute(route);
		
		this.requestRouteIdPromise(routeDetails)
			.then(() => this.updateDrivingDirectionPromise())
			.then(() => $('#destinationsForm').hide())
			.then(() => disableAllDestinationButtons())
			.then(() => enableSpeedButtons(destinationSignal))
			.then(() => this.enableReachedDestinationButton())
			.then(() => this.driveRoutePromise())
			.then(() => {
				if (!this.hasValidTrainSession) {
					this.clearDestinationReachedInterval();
					throw new Error("Game has ended");
				}
			})
			.then(() => this.updateCurrentBlock())
			.then(() => updatePossibleRoutes(driver.currentBlock))
			.then(() => disableSpeedButtons())
			.then(() => disableReachedDestinationButton())
			.catch(() => { });
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
			if (responseData.includes("grabbed: no") && !responseData.includes("on segment: no")) {
				success();
			} else {
				error();
			}
		}
	});
}

function startGameLogic() {
	// FIXME: On iOS, speech synthesis only works if it is first triggered by the user.
	speak("");

	if (driver.hasValidTrainSession) {
		setResponseDanger('#serverResponse', 'You are already driving a train!')
		return;
	}
	
	setResponseSuccess('#serverResponse', '⏳ Waiting ...');
				
	driver.grabTrainPromise()
		.then(() => $('#trainSelection').hide())
		.then(() => $('#endGameButton').show())
		.then(() => driver.updateCurrentBlock())
		.then(() => updatePossibleRoutes(driver.currentBlock))
		.always(() => driver.clearTrainAvailabilityInterval());
}

function endGameLogic() {
	$('#endGameButton').hide();
	$('#destinationsForm').hide();
	driver.clearRouteAvailabilityInterval();
	driver.clearDestinationReachedInterval();
	driver.reset();
	disableAllDestinationButtons();
	disableReachedDestinationButton();
	disableSpeedButtons();
	$('#trainSelection').show();
	driver.updateTrainAvailability();
}

// Wait for a duration in milliseconds.
function wait(duration) { 
	return new Promise(resolve => setTimeout(resolve, duration));
}

function initialise() {
	driver = new Driver(
		'master',                                 // trackOutput
		'libtrain_engine_default (unremovable)',  // trainEngine
		null                                      // trainId
	);
	
	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	// Update all train selections
	driver.updateTrainAvailability();


	//-----------------------------------------------------
	// Button behaviours
	//-----------------------------------------------------

	// Hide the train driving buttons (destinations and speed selections)
	$('#endGameButton').hide();	
	$('#destinationsForm').hide();
	disableReachedDestinationButton();
	disableSpeedButtons();

	// Handle train selection
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id;
		driver.trainId = trainId;
		startGameLogic();
	});
	
	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations = allPossibleDestinationsSwtbahnFull;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnStandard;
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
			if (!driver.endGameButtonIsPersistent) {
				$('#endGameButton').hide();
				$(window).bind("beforeunload", pageRefreshWarning);
			}
		});
	});

	$('#endGameButton').click(function () {
		if (!driver.hasValidTrainSession) {
			endGameLogic();
			return;
		}
		
		setResponseSuccess('#serverResponse', '⏳ Waiting ...');
					
		driver.setTrainSpeedPromise(0)
			.then(() => wait(500))
			.then(() => driver.releaseRoutePromise())
			.always(() => {
				driver.releaseTrainPromise();
				endGameLogic();
			});

		setResponseSuccess('#serverResponse', '😀 Thank you for playing');
	});
}

$(document).ready(
	() => initialise()
);

// Page unload (refresh or close) behaviour
function pageRefreshWarning(event) {
	event.preventDefault();
	console.log("Before unloading");
	
	// Most web browsers will display a generic message instead!!
	const message = "Are you sure you want to refresh or leave this page? " + 
					"Leaving this page without ending your game will prevent others from grabbing your train 😕";
	return event.returnValue = message;
}

$(window).on("unload", (event) => {
	// Exit game logic
	console.log("Unloading");
	if ($('#endGameButton').is(":visible")) {
		$('#endGameButton').click();
	}
});
