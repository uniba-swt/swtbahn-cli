/*************************************************************
 * Global state vars
 */

var driver_ = null;           // Train driver logic.
var drivingTimer_ = null;     // Timer for driving a train.
var trainSelector_ = null;    // Train selector logic.
var serverAddress_ = "";      // The base address of the server.
var language_ = "";           // User interface language.


/**************************************************
 * Destination related information and UI elements
 */

const numberOfDestinationsMax_ = 42;           // Maximum destinations to display
const destNamePrefix_ = "destination";         // HTML element ID prefix of the destination buttons

var allPossibleDestinations_ = null;           // Platform specific lookup table for destinations
var signalFlagMap_ = null;                     // Platform specific lookup table for signal flags

var defaultInterval_ = 5000;

// Returns the destinations possible from a given block
function getDestinations_adv(blockId) {
	if (allPossibleDestinations_ == null || !allPossibleDestinations_.hasOwnProperty(blockId)) {
		return null;
	}
	return allPossibleDestinations_[blockId];
}

function unpackRoute_adv(route) {
	for (let destinationSignal in route) {
		return [destinationSignal, route[destinationSignal]];
	}
	return null;
}

function disableAllDestButtons_adv() {
	console.log("Disable All Destination Buttons");
	driver_.clearUpdatePossibleDestsInterval_adv();
	for (let i = 0; i < numberOfDestinationsMax_; i++) {
		$(`#${destNamePrefix_}${i}`).val("");
		$(`#${destNamePrefix_}${i}`).attr("class", "flagThemeBlank");
	}
}

function setDestinationButton_adv(index, route) {
	const [destinationSignal, routeDetails] = unpackRoute_adv(route);
	console.log("setDestinationButton_adv: Index " + index + "; destinationSignal " + destinationSignal);
	const destination = destinationSignal.replace(/(a|b)$/, '');

	// Route details are stored in the value parameter of the destination button
	$(`#${destNamePrefix_}${index}`).val(JSON.stringify(route));
	$(`#${destNamePrefix_}${index}`).attr("class", signalFlagMap_[destination]);
}

function setDestinationButtonAvailable_adv(index, route) {
	setDestinationButton_adv(index, route);
	$(`#${destNamePrefix_}${index}`).removeClass("flagThemeDisabled");
}

function setDestinationButtonUnavailable_adv(index, route) {
	setDestinationButton_adv(index, route);
	$(`#${destNamePrefix_}${index}`).addClass("flagThemeDisabled");
}

/**
 * @member {Map} route
 * @member {number} index
 * @member {number} routeId
 */
class RouteChoice {
	route = { };
	index = -1;
	routeId = -1;
	
	constructor(route, index, routeId) {
		this.route = route;
		this.index = index;
		this.routeId = routeId;
	}
}

function updatePossibleDestinations_adv(blockId) {
	const routes = getDestinations_adv(blockId);
	if (routes == null) {
		console.log("Routes = null, return from updatePossibleDestinations_adv");
		return;
	}
	let rcMap = new Map();
	Object.keys(routes).forEach((destinationSignal, index) => {
		const route = { };
		route[destinationSignal] = routes[destinationSignal];
		const [_destinationSignal, routeDetails] = unpackRoute_adv(route);
		let routeId = routeDetails["route-id"];
		let rc = new RouteChoice(route, index, routeId);
		rcMap[routeId.toString()] = rc;
	});
	getDestinationStatusAndUpdateView_adv(rcMap);
}

function activateUpdatePossibleDestinationsInterval_adv(blockId) {
	console.log("Activating the interval for checking destinations for block " + blockId);
	$('#destinationsForm').show();
	disableAllDestButtons_adv();

	// Update once first
	updatePossibleDestinations_adv(blockId);
	
	// Set up a timer interval to periodically update the availability
	const updatePossibleDestinationsTimeout = defaultInterval_;
	driver_.updatePossibleDestsInterval = setInterval(() => {
		updatePossibleDestinations_adv(blockId);
	}, updatePossibleDestinationsTimeout);
}

/**
 * @param {string} routeInfo 
 * @returns {string}
 */
function getRouteIdFromRouteInfo_adv(routeInfo) {
	const regexMatch = /route id: (\w+)/g.exec(routeInfo);
	if (regexMatch != null && regexMatch.length >= 1) {
		return regexMatch[0];
	}
	return "";
}

/**
 * @param {string} routeInfo 
 * @returns {boolean}
 */
function isRouteAvailable_adv(routeInfo) {
	const isNotConflicting = routeInfo.includes("granted conflicting route ids: none");
	const isRouteClear = routeInfo.includes("route clear: yes");
	const isNotGranted = routeInfo.includes("granted train: none");
	return isNotConflicting && isRouteClear && isNotGranted;
}

/**
 * @param {Map<string,RouteChoice>} routeChoiceMap 
 */
function routeChoiceMapToIdList(routeChoiceMap) {
	rtlist = "";
	counter = 0;
	for (let k in routeChoiceMap.keys()) {
		counter++;
		rtlist += k;
		if (counter < routeChoiceMap.size) {
			rtlist += ","
		}
	}
	return rtlist;
}

/**
 * @param {string} routeInfoStr 
 * @param {Map<string,RouteChoice>} routeChoiceMap 
 */
function updateDestinationButtonFromRouteInfo(routeInfoStr, routeChoiceMap) {
	if (routeInfoStr != null && routeInfoStr.length > 0) {
		const rId = getRouteIdFromRouteInfo_adv(routeInfo);
		console.log("Extracted route id: " + rId);
		if (rId != "") {
			const rc = routeChoiceMap.get(rId);
			if (isRouteAvailable_adv(routeInfoStr)) {
				setDestinationButtonAvailable_adv(rc.index, rc.route);
			} else {
				setDestinationButtonUnavailable_adv(rc.index, rc.route);
			}
		}
	}
}

/**
 * @param {Map<string,RouteChoice>} routeChoiceMap 
 */
function getDestinationStatusAndUpdateView_adv(routeChoiceMap) {
	rtlist = routeChoiceMapToIdList(routeChoiceMap);
	$.ajax({
		type: 'POST',
		url: serverAddress_ + '/monitor/routes-by-ids',
		crossDomain: true,
		data: {
			'route-ids': rtlist,
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const responseDataSplit = responseData.split(';');
			for (let i = 0; i < responseDataSplit.length; i++) {
				routeInfo = responseDataSplit[i];
				updateDestinationButtonFromRouteInfo(routeInfo, routeChoiceMap);
			}
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log("/monitor/routes-by-ids err, response: " + responseData);
			setResponseDanger_adv('#serverResponse', 
				'There was a problem checking the destinations', 
				'Es ist ein Problem beim Überprüfen der Ziele aufgetreten'
			);
		}
	});
}

/**************************************************
 * Various
 */

function setChosenTrain_adv(trainId) {
	const imageName = `train-${trainId.replace("_", "-")}.jpg`
	$("#chosenTrain").attr("src", imageName);
}

function trainStateCallbackPromise_adv(trainId, successFunc, errorFunc) {
	return $.ajax({
		type: 'POST',
		url: serverAddress_ + '/monitor/train-state',
		crossDomain: true,
		data: { 'train': trainId },
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			successFunc(trainId, responseData);
		},
		error: (responseData, textStatus, errorThrown) => {
			errorFunc(trainId, responseData);
		}
	});
}

// Server request for a train's status and then execute the callback handlers
function trainIsAvailablePromise_adv(trainId, success, error) {
	return trainStateCallbackPromise_adv(trainId,
		(_t_id, response) => { // Success
			if (response.includes("grabbed: no") && !response.includes("on segment: no")) {
				success();
			} else {
				error();
			}
		}, (_t_id, _response) => { // Err
			console.log("trainIsAvailablePromise_adv: /monitor/train-state error case");
		;}
	);
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

function disableSpeedButtons_adv() {
	$('#drivingForm').hide();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', true);
	});
	drivingTimer_.stop();
}

function enableSpeedButtons_adv(destination) {
	$('#drivingForm').show();
	speedButtons.forEach(speed => {
		$(`#${speed}`).prop('disabled', false);
	});
	drivingTimer_.start();
}

function clearChosenDestination_adv() {
	$('#destination').attr("class", "flagThemeBlank");
}

function setChosenDestination_adv(destination) {
	destination = destination.replace(/(a|b)$/, '');
	$('#destination').attr("class", signalFlagMap_[destination]);
}

// ################################################################################################
//       Skipped the response/modal stuff to some degree

var responseTimer_ = null;

function setResponse_adv(responseId, messageEn, messageDe, callback) {
	clearTimeout(responseTimer_);

	const message = (language_ == 'en') ? messageEn : messageDe;
	$(responseId).text(message);
	callback();

	$(responseId).parent().fadeIn("fast");
	const responseTimeout = 7000;
	responseTimer_ = setTimeout(() => {
		$(responseId).parent().fadeOut("slow");
	}, responseTimeout);
}

function setResponseDanger_adv(responseId, messageEn, messageDe) {
	setResponse_adv(responseId, messageEn, messageDe, function() {
		$(responseId).parent().addClass('alert-danger');
		$(responseId).parent().addClass('alert-danger-blink');
		$(responseId).parent().removeClass('alert-success');
	});
}

function setResponseSuccess_adv(responseId, messageEn, messageDe) {
	setResponse_adv(responseId, messageEn, messageDe, function() {
		$(responseId).parent().removeClass('alert-danger');
		$(responseId).parent().removeClass('alert-danger-blink');
		$(responseId).parent().addClass('alert-success');
	});
}

// ################################################################################################


class TrainSelector {
	trainAvailabilityInterval = null;
	trainAvailabilityTimeout = defaultInterval_;
	
	constructor() {}
	
	clearTrainAvailInterval_adv() {
		console.log("clearTrainAvailInterval_adv");
		clearInterval(this.trainAvailabilityInterval);
	}
	
	checkTrainAvailability_adv() {
		// Enable a train if it is on the tracks and has not been grabbed
		$('.selectTrainButton').each((_index, obj) => {
			let trainId = obj.id;
			trainIsAvailablePromise_adv(trainId,
				() => {  // Train available
					$(obj).prop("disabled", false);
					$(obj).removeClass("btn-danger");
					$(obj).addClass("btn-primary");
					$($(obj).parent(".card-body").parent(".card")).removeClass("unavailableTrain");
					$(obj).children().each(function() {
						switch($(this).attr('lang')){
							case "de": $(this).text("Fahre diesen Zug"); break;
							case "en": $(this).text("Drive this train"); break;
						}
					});
				},
				() => { // Train not available
					$(obj).prop("disabled", true);
					$(obj).removeClass("btn-primary");
					$(obj).addClass("btn-danger");
					$($(obj).parent(".card-body").parent(".card")).addClass("unavailableTrain");
					$(obj).children().each(function() {
						switch($(this).attr('lang')){
							case "de": $(this).text("Nicht verfügbar"); break;
							case "en": $(this).text("Unavailable"); break;
						}
					});
				}
			);
		})
	}
	
	activateTrainAvailCheckInterval_adv() {
		console.log("Activating Train availability checking interval");
		$('.selectTrainButton').prop("disabled", true);
		this.checkTrainAvailability_adv();
		this.trainAvailabilityInterval = setInterval(() => {
			this.checkTrainAvailability_adv();
		}, this.trainAvailabilityTimeout);
	}
}

/**************************************************
 * Driver class that controls the driving of a
 * train and the UI elements
 */

class DriverAdv {
	// Server and train details
	sessionId = 0;
	trackOutput = "master";
	trainEngine = "libtrain_engine_default (unremovable)";
	trainId = "";
	grabId = -1;

	// Driving details
	routeDetails = null;
	drivingIsForwards = true;
	currentBlock = "";
	isDestinationReached = false;

	// Timer intervals
	updatePossibleDestsInterval = null;
	
	constructor(trainId) {
		this.trainId = trainId;
		drivingTimer_ = new Timer();
	}
	
	reset_adv() {
		this.sessionId = 0;
		this.grabId = -1;
	}
	
	hasValidTrainSession_adv() {
		return (this.sessionId != 0 && this.grabId != -1);
	}
	
	hasRouteGranted_adv() {
		return (this.routeDetails != null);
	}
	
	clearUpdatePossibleDestsInterval_adv() {
		console.log("Stopping Destination Update Interval");
		clearInterval(this.updatePossibleDestsInterval);
	}
	
	// Server request for the train's current block
	updateCurrentBlockPromise_adv() {
		return trainStateCallbackPromise_adv(this.trainId,
			(_t_id, response) => { // Success
				const regexMatch = /on block: (.*?) /g.exec(response);
				if (regexMatch != null && regexMatch.length >= 1) {
					this.currentBlock =  regexMatch[1];
				}
			}, (_t_id, _response) => { // Err
				setResponseDanger_adv('#serverResponse', 
					'Could not find your train', 
					'Dein Zug konnte nicht gefunden werden'
				);
			}
		);
	}
	
	// request server to grab a train
	grabTrainPromise_adv() {
		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/driver/grab-train',
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
				setResponseSuccess_adv('#serverResponse', 'Your train is ready', 'Dein Zug ist bereit');
			},
			error: (responseData, textStatus, errorThrown) => {
				console.log("grabTrainPromise_adv: /driver/grab-train err case, response: " + responseData);
				setResponseDanger_adv('#serverResponse', 
					'There was a problem starting your train', 
					'Fehler bei der Zugauswahl'
				);
			}
		});
	}
	
	// request server for the train's physical driving direction of its granted route
	updateDrivingDirectionPromise_adv() {
		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/driver/direction',
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
				console.log("updateDrivingDirectionPromise_adv: /driver/direction err case, response: " + responseData);
				setResponseDanger_adv('#serverResponse', 
					'Train direction could not be determined',
					'Fahrtrichtung konnte nicht bestimmt werden'
				);
			}
		});
	}
	
	
	// request server to set the train's speed
	setTrainSpeedPromise_adv(speed) {
		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/driver/set-dcc-train-speed',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'speed': this.drivingIsForwards ? speed : "-" + speed,
				'track-output': this.trackOutput
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				console.log("Speed was successfully set to " + speed);
				// If speed was not 0, and routeDetails are null by now, set the speed to 0
				if (speed != 0 && this.routeDetails == null) {
					console.log("Set train speed success: route now null and set speed was not 0, brake now.");
					this.setTrainSpeedPromise_adv(0);
				}
			},
			error: (responseData, textStatus, errorThrown) => {
				console.log("setTrainSpeedPromise_adv: /driver/set-dcc-train-speed err case, response: " + responseData);
				setResponseDanger_adv('#serverResponse', 
					'There was a problem setting the speed of your train', 
					'Zuggeschwindigkeit konnte nicht eingestellt werden'
				);
			}
		});
	}
	
	// Server request to release the train
	releaseTrainPromise_adv() {
		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/driver/release-train',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.reset_adv();
				this.trainId = null;
			},
			error: (responseData, textStatus, errorThrown) => {
				console.log("releaseTrainPromise_adv: /driver/release-train err case, response: " + responseData);
				setResponseDanger_adv('#serverResponse', 
					'There was a problem ending your turn', 
					'Es ist ein Problem beim Beenden aufgetreten'
				);
			}
		});
	}
	
	requestRouteIdPromise_adv(routeDetails) {
		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/driver/request-route-id',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'route-id': routeDetails['route-id']
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				this.routeDetails = routeDetails;
				setResponseSuccess_adv('#serverResponse', 
					'Start driving your train to your chosen destination', 
					'Fahr deinen Zug zum ausgewählten Ziel',
				);
			},
			error: (responseData, textStatus, errorThrown) => {
				console.log("requestRouteIdPromise_adv: /driver/request-route-id err case, response: " + responseData);
				this.routeDetails = null;
				setResponseDanger_adv('#serverResponse', "This route is not available", 
				"Dieses Ziel ist zurzeit nicht verfügbar");
			}
		});
	}
	
	// request server to let driver drive the granted route
	driveRoutePromise_adv() {
		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/driver/drive-route-guarded',
			crossDomain: true,
			data: {
				'session-id': this.sessionId,
				'grab-id': this.grabId,
				'route-id': this.routeDetails["route-id"]
			},
			dataType: 'text',
			success: (responseData, textStatus, jqXHR) => {
				console.log("drive-route-guarded returned success with response: " + responseData);
				this.isDestinationReached = true;
				if (!this.hasValidTrainSession_adv()) {
					// Ignore, driver has ended their trip?
					return;
				}
				// The page can be refreshed without ill consequences.
				$(window).unbind('beforeunload', pageRefreshWarning);
				$('#endGameButton').show();
				this.routeDetails = null;
				if (responseData === "OKAY_STOPPED_AT_ROUTE_END") {
					//setModalSuccess(modalMessages.drivingSuccess, 'Juhuu!!');
				} else {
					///TODO: Adjust based on returned code
					// Copy the driving infringement message and fill in the destination flag
					const destinationClass = $('#destination').attr('class');
					let drivingInfringement = JSON.parse(JSON.stringify(modalMessages.drivingInfringement));
					drivingInfringement['body']['de'] = drivingInfringement['body']['de'].replace('${destination}', destinationClass);
					drivingInfringement['body']['en'] = drivingInfringement['body']['en'].replace('${destination}', destinationClass);
					//setModalDanger(drivingInfringement, 'STOP, STOP, STOP');
				}
			},
			error: (responseData, textStatus, errorThrown) => {
				console.log("driveRoutePromise_adv: /driver/drive-route-guarded err case, response: " + responseData);
				setResponseDanger_adv('#serverResponse', 
					'Route to your chosen destination is unavailable', 
					'Die Route zum ausgewählten Ziel ist aktuell nicht verfügbar'
				);
			}
		});
	}
	
	// Server request to release the granted route
	releaseRoutePromise_adv() {
		if (!this.hasRouteGranted_adv()) {
			return;
		}
		
		const routeId = this.routeDetails["route-id"];
		this.routeDetails = null;

		return $.ajax({
			type: 'POST',
			url: serverAddress_ + '/controller/release-route',
			crossDomain: true,
			data: { 'route-id': routeId },
			dataType: 'text'
		});
	}
	
	
	// Manage the business logic of manually driving a granted route
	async requestAndDriveRoutePromise_adv(route) {
		if (!this.hasValidTrainSession_adv()) {
			setResponseDanger_adv('#serverResponse', 'Could not find your train', 'Kontrolle über Zug verloren, bitte neu laden');
			return;
		}
		const lock = await Mutex.lock();
		
		if (this.hasRouteGranted_adv()) {
			// Already has a route -> unlock and return
			Mutex.unlock(lock);
			return;
		}
		
		this.isDestinationReached = false;
		const [destinationSignal, routeDetails] = unpackRoute_adv(route);
        console.log("requestAndDriveRoutePromise_adv: " + routeDetails);
        
		this.requestRouteIdPromise(routeDetails)                       // 1. Ensure that the chosen destination is still available
			.then(() => this.updateDrivingDirectionPromise_adv())      // 2. Obtain the physical driving direction
			.then(() => $('#destinationsForm').hide())                 // 3. Prevent the driver from choosing another destination
			.then(() => disableAllDestButtons_adv())
			.then(() => Mutex.unlock(lock))
			.then(() => this.setTrainSpeedPromise_adv(5))              // 4. Update the train lights to indicate the physical driving direction
			.then(() => wait_adv(50))
			.then(() => this.setTrainSpeedPromise_adv(0))
			.then(() => setChosenDestination_adv(destinationSignal))   // 5. Show the chosen destination and possible train speeds to the driver
			.then(() => enableSpeedButtons_adv(destinationSignal))

			.then(() => this.driveRoutePromise_adv())                  // 7. Start the manual driving mode for the granted route

			.then(() => disableSpeedButtons_adv())                     // 8. Prevent the driver from driving past the destination
			.then(() => clearChosenDestination_adv())

			.then(() => {
				if (!this.hasValidTrainSession_adv()) {                      // 9. Check whether the driver has quit their session
					throw new Error("Driver ended their session");
				}
			})
			.then(() => this.updateCurrentBlockPromise_adv())          // 11. Show the next possible destinations
			.then(() => activateUpdatePossibleDestinationsInterval_adv(driver_.currentBlock))
			.catch(() => Mutex.unlock(lock));                          // 12. Execution skips to here if the safety layer was triggered (step 9)
	}
}



/*************************************************************
 * UI update for client initialisation and game start and end
 * (train grab and release)
 */

// Update the user interface for driving when the user decides to grab a train
function startGameLogic_adv() {
	if (driver_.hasValidTrainSession_adv()) {
		setResponseDanger_adv('#serverResponse', 'You are already driving a train!', 'Du fährst schon einen Zug!');
		return;
	}

	setResponseSuccess_adv('#serverResponse', '⏳ Waiting...', '⏳ Bitte Warten...');

	driver_.grabTrainPromise_adv()
		.then(() => $('#trainSelection').hide())
		.then(() => $('#chosenTrain').show())
		.then(() => $('#endGameButton').show())
		.then(() => driver_.updateCurrentBlockPromise_adv())
		.then(() => activateUpdatePossibleDestinationsInterval_adv(driver_.currentBlock))
		.always(() => trainSelector_.clearTrainAvailInterval_adv());
}

// Update the user interface for driving when the user decides to release their train
function endGameLogic_adv() {
	console.log("Pressed end game logic");
	$('#endGameButton').hide();
	$('#destinationsForm').hide();
	$('#chosenTrain').hide();
	driver_.isDestinationReached = false;
	
	driver_.clearUpdatePossibleDestsInterval_adv();
	disableAllDestButtons_adv();
	disableSpeedButtons_adv();
	clearChosenDestination_adv();
	$('#trainSelection').show();
}

// Asynchronous wait for a duration in milliseconds.
function wait_adv(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
}

function initialise_language_adv() {
	// Set the initial language.
	$('span:lang(en)').hide();
	$('span:lang(de)').show();
	language_ = 'de';
	// Handle language selection.
	$('#changeLang').click(function () {
		$('span:lang(en)').toggle();
		$('span:lang(de)').toggle();
		language_ = (language_ == 'en') ? 'de' : 'en';
	});
}

function initialise_destinations_adv() {
	// Hide the train driving buttons (destination selections).
	$('#destinationsForm').hide();
	clearChosenDestination_adv();
	// Set the possible destinations for the SWTbahn platform.
	allPossibleDestinations_ = allPossibleDestinationsSwtbahnFull;
	// allPossibleDestinations_ = allPossibleDestinationsSwtbahnStandard;
	// allPossibleDestinations_ = allPossibleDestinationsSwtbahnUltraloop;
	disableAllDestButtons_adv();
	
	// Set the signal to flag mapping.
	signalFlagMap_ = signalFlagMapSwtbahnFull;
	
	// Initialise the click handler of each destination button.
	for (let i = 0; i < numberOfDestinationsMax_; i++) {
		const destinationButton = $(`#${destNamePrefix_}${i}`);
		destinationButton.click(function () {
			// Should check if button is enabled here
			if (destinationButton.hasClass("flagThemeDisabled")) {
				console.log("Route Driving was not attempted because it is disabled");
				setResponseDanger_adv('#serverResponse', "This route is not available", "Diese Route ist zurzeit nicht verfügbar");
			} else {
				const route = JSON.parse(destinationButton.val());
				driver_.requestAndDriveRoutePromise_adv(route);
			}
		});
	}
}

function initialise_trainselection_adv() {
	// Handle train selection.
	$('.selectTrainButton').click(function (event) {
		let trainId = event.currentTarget.id;
		driver_.trainId = trainId;
		setChosenTrain_adv(trainId);
		startGameLogic_adv();
	});
}

function initialise_speedcontrol_adv() {
	disableSpeedButtons_adv();
	// Initialise the click handler of each speed button.
	speedButtons.forEach(speed => {
		const speedButton = $(`#${speed}`);
		speedButton.click(function () {
			driver_.setTrainSpeedPromise_adv(speedButton.val());
			if (!driver_.isDestinationReached) {
				$('#endGameButton').hide();
				driver_.updateCurrentBlockPromise_adv().then(
					() => {console.log("TODO: the *keep driving* model should appear here.");}
				);
				// The page cannot be refreshed without ill consequences.
				$(window).bind("beforeunload", pageRefreshWarning_adv);
			} else if (speedButton.val() == '0') {
				console.log("Driver set speed 0 at end of route");
			}
		});
	});
}

function initialise_endbutton_adv() {
	console.log("Registering endbutton click");
	$('#endGameButton').click(function () {
		console.log("Endbutton clicked");
		setResponseSuccess_adv('#serverResponse', '⏳ Waiting...', '⏳ Bitte Warten...');
		
		driver_.setTrainSpeedPromise_adv(0)
			.then(() => wait_adv(50))
			.then(() => {
				if (driver_.hasRouteGranted_adv()) {
					driver_.releaseRoutePromise_adv();
				}
			})
			.then(() => wait_adv(50))
			.then(() => driver_.releaseTrainPromise_adv())
			.then(() => wait_adv(50))
			.then(() => endGameLogic_adv());
		
		setResponseSuccess_adv('#serverResponse', 'Thank you for playing', 'Danke fürs Spielen');
	});
}


// Initialisation of the user interface and game logic
function initialise_adv() {
	driver_ = new DriverAdv(null);
	trainSelector_ = new TrainSelector();
	
	// Hide the "chosen train" element.
	$('#chosenTrain').hide();
	// Hide the alert box for displaying server messages.
	$('#serverResponse').parent().hide();
	
	trainSelector_.activateTrainAvailCheckInterval_adv();
	
	//-----------------------------------------------------
	// Configure button behaviours
	//-----------------------------------------------------
	initialise_language_adv();
	$('#endGameButton').hide();
	initialise_destinations_adv();
	initialise_trainselection_adv();
	initialise_speedcontrol_adv();
	initialise_endbutton_adv();
	$('#trainSelection').show();
}

$(document).ready(() => {
	initialise_adv();
});


/*************************************
 * Handlers for page refresh or close
 */

// Before page unload (refresh or close) behaviour
function pageRefreshWarning_adv(event) {
	event.preventDefault();
	console.log("Before unloading");

	// Most web browsers will display a generic message instead!!
	const message = "Are you sure you want to refresh or leave this page? " +
		"Leaving this page without ending your game will prevent others from grabbing your train";
	return event.returnValue = message;
}

// During page unload (refresh or close) behaviour
// Only synchronouse statements will be executed by this handler!
// Promises will not be executed!
$(window).on("unload", (event) => {
	console.log("Unloading");
	if (driver_.hasRouteGranted_adv()) {
		const formData = new FormData();
		formData.append('route-id', driver_.routeDetails['route-id']);
		navigator.sendBeacon(serverAddress_ + '/controller/release-route', formData);
	}
	if (driver_.hasValidTrainSession_adv()) {
		const formData = new FormData();
		formData.append('session-id', driver_.sessionId);
		formData.append('grab-id', driver_.grabId);
		navigator.sendBeacon(serverAddress_ + '/driver/release-train', formData);
	}
});
