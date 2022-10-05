var driver = null;           // Train driver logic.
var serverAddress = ""; //= "http://141.13.32.44:8080"; // The address of the server.


/**************************************************
 * Destination related information and UI elements
 */

const numberOfDestinationsMax = 12;           // Maximum destinations to display
const destinationNamePrefix = "destination";  // HTML element ID prefix of the destination buttons

var allPossibleDestinations = null;           // Platform specific lookup table for destinations
var signalFlagMap = null;                     // Platform specific lookup table for signal flags

// Returns the destinations possible from a given block
function getDestinations(blockId) {
	if (allPossibleDestinations == null || !allPossibleDestinations.hasOwnProperty(blockId)) {
		return null;
	}
	return allPossibleDestinations[blockId];
}

// Destructures a route into its destination signal and route details
function unpackRoute(route) {
	for (let destinationSignal in route) {
		return [destinationSignal, route[destinationSignal]];
	}
	return null;
}

function disableAllDestinationButtons() {
	driver.clearUpdatePossibleDestinationsInterval();

	for (let i = 0; i < numberOfDestinationsMax; i++) {
		$(`#${destinationNamePrefix}${i}`).val("");
		$(`#${destinationNamePrefix}${i}`).attr("class", "flagThemeBlank");
	}
}

function setDestinationButton(choice, route) {
	const [destinationSignal, routeDetails] = unpackRoute(route);
	const destination = destinationSignal.replace(/(a|b)$/, '');

	// Route details are stored in the value parameter of the destination button
	$(`#${destinationNamePrefix}${choice}`).val(JSON.stringify(route));
	$(`#${destinationNamePrefix}${choice}`).attr("class", signalFlagMap[destination]);
}

function setDestinationButtonAvailable(choice, route) {
	setDestinationButton(choice, route);
	$(`#${destinationNamePrefix}${choice}`).removeClass("flagThemeDisabled");
}

function setDestinationButtonUnavailable(choice, route) {
	setDestinationButton(choice, route);
	$(`#${destinationNamePrefix}${choice}`).addClass("flagThemeDisabled");
}

function showTrainHeader(trainId){
	$("#trainheader").empty();
	switch(trainId){
		case "cargo_db":
			$("#trainheader").append("<img id='trainHeaderView' class='img-fluid' src='train-cargo-db.jpg'>");
			break;
		case "cargo_green":
			$("#trainheader").append("<img id='trainHeaderView' class='img-fluid' src='train-cargo-green.jpg'>");
			break;
		default:
			$("#trainheader").append("<img id='trainHeaderView' class='img-fluid' src='train-regional-odeg.jpg'>");
			break;
	}
}

// Periodically update the availability of a blocks possible destinations.
// Update can be stopped by cancelling updatePossibleDestinationsInterval,
// e.g., when disabling the destination buttons or ending the game.
function updatePossibleDestinations(blockId) {
	// Show the form that contains all the destination buttons
	$('#destinationsForm').show();
	disableAllDestinationButtons();

	// Set up a timer interval to periodically update the availability
	const updatePossibleDestinationsTimeout = 500;
	driver.updatePossibleDestinationsInterval = setInterval(() => {
		console.log("Checking available destinations ...");

		const routes = getDestinations(blockId);
		if (routes == null) {
			return;
		}

		Object.keys(routes).forEach((destinationSignal, choice) => {
			const route = { };
			route[destinationSignal] = routes[destinationSignal];

			const [_destinationSignal, routeDetails] = unpackRoute(route);
			let routeId = routeDetails["route-id"];
			updateDestinationAvailabilityPromise(
				routeId,
				// route is available
				() => setDestinationButtonAvailable(choice, route),
				// route is unavailable
				() => setDestinationButtonUnavailable(choice, route)
			);
		});
	}, updatePossibleDestinationsTimeout);
}

// Server request for a route's status and then determine whether the
// route is available
function updateDestinationAvailabilityPromise(routeId, available, unavailable) {
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


/**************************************************
 * Train speed UI elements
 */

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

function enableSpeedButtons(destination) {
	$('#speedForm').show();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', false);
	});
}

function clearChosenDestination() {
	$('#destination').attr("class", "flagThemeBlank");
}

function setChosenDestination(destination) {
	destination = destination.replace(/(a|b)$/, '');
	$('#destination').attr("class", signalFlagMap[destination]);
}

function disableReachedDestinationButton() {
	$('#destinationReachedForm').hide();
	$('#destinationReached').prop('disabled', true);
	driver.endGameButtonIsPersistent = false;
}


/**************************************************
 * Feedback UI elements
 */
var responseTimer = null;

function setResponse(responseId, message, callback) {
	clearTimeout(responseTimer);

	$(responseId).text(message);
	callback();

	$(responseId).parent().fadeIn("fast");
	const responseTimeout = 7000;
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
	speak('STOP, NEIN, ACHTUNG!');
}

function setResponseSuccess(responseId, message) {
	setResponse(responseId, message, function() {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});
}


/**************************************************
 * Driver class that controls the driving of a
 * train and the UI elements
 */

class Driver {
	// Server and train details
	sessionId = null;
	trackOutput = null;
	trainEngine = null;
	trainId = null;
	grabId = null;

	// Driving details
	routeDetails = null;
	drivingIsForwards = null;
	currentBlock = null;
	endGameButtonIsPersistent = null;

	// Timer intervals
	trainAvailabilityInterval = null;
	updatePossibleDestinationsInterval = null;
	destinationReachedInterval = null;

	constructor(trackOutput, trainEngine, trainId) {
		this.sessionId = 0;
		this.trackOutput = trackOutput;
		this.trainEngine = trainEngine;
		this.trainId = trainId;
		this.grabId = -1;

		this.routeDetails = null;
		this.drivingIsForwards = null;
		this.currentBlock = null;
		this.endGameButtonIsPersistent = false;

		this.trainAvailabilityInterval = null;
		this.updatePossibleDestinationsInterval = null;
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

	clearUpdatePossibleDestinationsInterval() {
		console.log("clearUpdatePossibleDestinationsInterval");
		clearInterval(this.updatePossibleDestinationsInterval);
	}

	clearDestinationReachedInterval() {
		console.log("clearDestinationReachedInterval");
		clearInterval(this.destinationReachedInterval);
	}

	// Server request for the train's current block
	updateCurrentBlockPromise() {
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

	// Server request for a train's status and then execute the callback handlers
	trainIsAvailablePromise(trainId, success, error) {
		return $.ajax({
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
			},
			error: (responseData, textStatus, errorThrown) => {
				// Do nothing
			}
		});
	}

	// Update the styling of the train selection buttons based on the train availabilities
	updateTrainAvailability() {
		$('.selectTrainButton').prop("disabled", true);
		const trainAvailabilityTimeout = 1000;
		this.trainAvailabilityInterval = setInterval(() => {
			console.log("Checking available trains ... ");

			// Enable a train if it is on the tracks and has not been grabbed
			$('.selectTrainButton').each((index, obj) => {
				let trainId = obj.id;
				this.trainIsAvailablePromise(
					trainId,
					() => $(obj).prop("disabled", false),
					() => $(obj).prop("disabled", true)
				);
			})
		}, trainAvailabilityTimeout);
	}

	// Server request to grab a train
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

	// Server request for the train's physical driving direction of its granted route
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

	// Server request to set the train's speed
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

	// Server request for the train's current segment and then determine 
	// whether to show the destination reached button
	enableReachedDestinationButtonPromise() {
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

					// Show the destination reached button when the train is only on the
					// main segment of the destination
					if (segments.length != 1) {
						return;
					}
					// Take into account that a main segment could be split into a/b segments
					const segment = segments[0].replace(/(a|b)$/, '');
					if(segment == this.routeDetails["segment"]) {
						this.clearDestinationReachedInterval();
						this.endGameButtonIsPersistent = true;
						$('#endGameButtonBox').show();
						$('#destinationReachedForm').show();
						$('#destinationReached').prop('disabled', false);

						// The page can be refreshed without ill consequences.
						// The train will stop sensibly on the main segment of the destination
						$(window).unbind("beforeunload", pageRefreshWarning);
					}
				}
			});
		}, destinationReachedTimeout);
	}

	// Server request to release the train
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

	// Server request for a specific route ID
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

	// Server request to manually drive the granted route
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
				if (!this.hasValidTrainSession) {
					// Ignore, end game was called
				} else if (!this.hasRouteGranted) {
					setResponseSuccess('#serverResponse', '🥳 You drove your train to your chosen destination');
				} else {
					setResponseDanger('#serverResponse', '👎 You did not stop your train before the destination signal!');
				}
			},
			error: (responseData, textStatus, errorThrown) => {
				setResponseDanger('#serverResponse', '😢 Route to your chosen destination is unavailable');
			}
		});
	}

	// Server request to release the granted route
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

	// Manage the business logic of manually driving a granted route
	driveToPromise(route) {
		if (!this.hasValidTrainSession) {
			setResponseDanger('#serverResponse', 'Your train could not be found 😢');
			return;
		}

		const [destinationSignal, routeDetails] = unpackRoute(route);

		this.requestRouteIdPromise(routeDetails)                       // 1. Ensure that the chosen destination is still available
			.then(() => this.updateDrivingDirectionPromise())          // 2. Obtain the physical driving direction
			.then(() => $('#destinationsForm').hide())                 // 3. Prevent the driver from choosing another destination
			.then(() => disableAllDestinationButtons())
			.then(() => this.setTrainSpeedPromise(1))                  // 4. Update the train lights to indicate the physical driving direction
			.then(() => this.setTrainSpeedPromise(0))
			.then(() => setChosenDestination(destinationSignal))       // 5. Show the chosen destination and possible train speeds to the driver
			.then(() => enableSpeedButtons(destinationSignal))
			.then(() => this.enableReachedDestinationButtonPromise())  // 6. Start monitoring whether the train has reached the destination

			.then(() => this.driveRoutePromise())                      // 7. Start the manual driving mode for the granted route

			.then(() => disableSpeedButtons())                         // 8. Prevent the driver from driving past the destination
			.then(() => clearChosenDestination())

			.then(() => {
				if (!this.hasValidTrainSession) {                      // 9. Check whether the safety layer had to force the train to stop
					this.clearDestinationReachedInterval();
					throw new Error("Game has ended");
				}
			})
			.then(() => disableReachedDestinationButton())             // 10. Remove the destination reached button
			.then(() => this.updateCurrentBlockPromise())              // 11. Show the next possible destinations
			.then(() => updatePossibleDestinations(driver.currentBlock))
			.catch(() => { });                                         // 12. Execution skips to here if the safety layer was triggered (step 9)
	}
}


/*************************************************************
 * UI update for client initialisation and game start and end
 * (train grab and release)
 */

// Update the user interface for driving when the user decides to grab a train
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
		.then(() => $('#trainheader').show())
		.then(() => $('#endGameButtonBox').show())
		.then(() => driver.updateCurrentBlockPromise())
		.then(() => updatePossibleDestinations(driver.currentBlock))
		.always(() => driver.clearTrainAvailabilityInterval());
}

// Update the user interface for driving when the user decides to release their train
function endGameLogic() {
	$('#endGameButtonBox').hide();
	$('#destinationsForm').hide();
	driver.clearUpdatePossibleDestinationsInterval();
	driver.clearDestinationReachedInterval();
	driver.reset();
	disableAllDestinationButtons();
	disableReachedDestinationButton();
	disableSpeedButtons();
	clearChosenDestination();

	$('#trainSelection').show();
	$('#trainheader').hide();
	driver.updateTrainAvailability();
}

// Asynchronous wait for a duration in milliseconds.
function wait(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
}

// Initialisation of the user interface and game logic
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
	// Attach button behaviours
	//-----------------------------------------------------

	// Hide the train driving buttons (destination selections)
	$('#endGameButtonBox').hide();
	$('#destinationsForm').hide();
	clearChosenDestination();
	disableReachedDestinationButton();

	// Handle train selection
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id;
		driver.trainId = trainId;
		showTrainHeader(trainId);
		startGameLogic();
	});

	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations = allPossibleDestinationsSwtbahnFull;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnStandard;
//	allPossibleDestinations = allPossibleDestinationsSwtbahnUltraloop;
	disableAllDestinationButtons();

	// Set the signal to flag mapping.
	signalFlagMap = signalFlagMapSwtbahnFull;

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
				$('#endGameButtonBox').hide();

				// The page cannot be refreshed without ill consequences.
				// The train might not stop sensibly on the main segment of the destination
				$(window).bind("beforeunload", pageRefreshWarning);
			}
		});
	});

	$('#endGameButtonBox').click(function () {
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


$(document).ready(() => {
	initialise();
});

/*************************************
 * Handlers for page refresh or close
 */

// Before page unload (refresh or close) behaviour
function pageRefreshWarning(event) {
	event.preventDefault();
	console.log("Before unloading");

	// Most web browsers will display a generic message instead!!
	const message = "Are you sure you want to refresh or leave this page? " +
	                "Leaving this page without ending your game will prevent others from grabbing your train 😕";
	return event.returnValue = message;
}

// During page unload (refresh or close) behaviour
// Only synchronouse statements will be executed by this handler!
// Promises will not be executed!
$(window).on("unload", (event) => {
	console.log("Unloading");
	if (driver.hasRouteGranted) {
		const formData = new FormData();
		formData.append('route-id', driver.routeDetails['route-id']);
		navigator.sendBeacon(serverAddress + '/controller/release-route', formData);
	}
	if (driver.hasValidTrainSession) {
		const formData = new FormData();
		formData.append('session-id', driver.sessionId);
		formData.append('grab-id', driver.grabId);
		navigator.sendBeacon(serverAddress + '/driver/release-train', formData);
	}
});
