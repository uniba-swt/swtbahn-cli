var trackOutput = 'master';
var sessionId = 0;
var grabId = -1;
var trainId = '';
var trainEngine = '';
var verificationObj = {};
var trainIsForwards = true;
var lastSetSpeed = 0;			// Only needed when swapping train direction

var customCode_500ServerUnableReplyMsg = {500: "Server unable to build reply message"};

const speeds = [
	"0",
	"10",
	"20",
	"30",
	"40",
	"50",
	"60",
	"70",
	"80",
	"90",
	"100",
	"110",
	"120",
	"126"
]

// Helpers

function showInfoInElem(elementID, textToShow) {
	$(elementID).parent().removeClass('alert-danger');
	$(elementID).parent().addClass('alert-success');
	$(elementID).text(textToShow);
}

function showErrorInElem(elementID, textToShow) {
	$(elementID).parent().removeClass('alert-success');
	$(elementID).parent().addClass('alert-danger');
	$(elementID).text(textToShow);
}

function getErrorMessage(jqXHR, customCodes = {}) {
	try {
		if (jqXHR.responseText.length > 0) {
			const respJson = JSON.parse(jqXHR.responseText);
			// If response has a "msg" field, that's the error msg (by convention in swtbahn-cli)
			if (respJson.msg) {
				return respJson.msg;
			}
		}
	} catch (parseError) {
		console.log("Unable to parse jqXHR to JSON.");
	}

	// Check if customCodes contains the status code
	if (customCodes.hasOwnProperty(jqXHR.status)) {
		return customCodes[jqXHR.status];
	}

	// Fallback to default messages
	switch (jqXHR.status) {
		case 405:
			return "Method not allowed";
		case 503:
			return "SWTbahn not running";
		default:
			return `${jqXHR.status} - Error message not defined`;
	}
}

//From https://github.com/eligrey/FileSaver.js/wiki/FileSaver.js-Example
function SaveAsFile(content, filename, contentTypeOptions) {
	try {
		var b = new Blob([content], { type: contentTypeOptions });
		saveAs(b, filename);
	} catch (e) {
		console.log("SaveAsFile Failed to save the file. Error msg: " + e);
	}
}

// State Updates

function updateTrainIsForwards() {
	$.ajax({
		type: 'POST',
		url: '/monitor/train-state',
		crossDomain: true,
		data: { 'train': trainId },
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			trainIsForwards = responseJson.direction === 'forwards';
		}
	});
}

function updateTrainGrabbedState() {
	// Called from client.html
	return $.ajax({
		type: 'GET',
		url: '/monitor/trains',
		crossDomain: true,
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			responseJson['trains'].forEach((train) => {
				const trainId = train.id;
				const isGrabbed = train.grabbed;
				if (isGrabbed) {
					$(`#releaseTrainButton_${trainId}`).show();
				} else {
					$(`#releaseTrainButton_${trainId}`).hide();
				}
			});
		}
	});
}

function getPointsWithCallbacks(successCallback, errorCallback) {
	return $.ajax({
		type: 'GET',
		url: '/monitor/points',
		crossDomain: true,
		dataType: 'text',
		success: (responseText) => {
			successCallback(responseText);
		},
		error: (jqXHR) => {
			errorCallback(jqXHR);
		}
	});
}

function addPointOptionsForSelectElems(htmlSelectElemIDs) {
	return getPointsWithCallbacks(
		// success callback
		(responseText) => {
			let responseJson = JSON.parse(responseText);
			// Filter by existance
			htmlSelectElemIDs = htmlSelectElemIDs.filter(selectElemID => $(selectElemID).length);
			// clear the existing options
			htmlSelectElemIDs.forEach(selectElemID => {
				$(selectElemID).get(0).replaceChildren();
			});
			responseJson.points.sort((a, b) => {
				let numA = parseInt(a.id.replace(/[^0-9]/g, ''));
				let numB = parseInt(b.id.replace(/[^0-9]/g, ''));
				return numA > numB;
			});
			// add option for each point for each select
			for (const point of responseJson.points) {
				htmlSelectElemIDs.forEach(selectElemID => {
					let optElem = document.createElement("option");
					optElem.text = point.id;
					$(selectElemID).get(0).add(optElem);
				});
			}
		},
		(responseText, status) => {
			console.log(`Error reading points of the SWTbahn platform, code ${jqXHR.status}, text ${jqXHR.responseText}`);
			showErrorInElem("#loadAccessoriesResponse", getErrorMessage(jqXHR));
		}
	);
}

function getSignalsWithCallbacks(successCallback, errorCallback) {
	return $.ajax({
		type: 'GET',
		url: '/monitor/signals',
		crossDomain: true,
		dataType: 'text',
		success: (responseText) => {
			successCallback(responseText);
		},
		error: (jqXHR) => {
			errorCallback(jqXHR);
		}
	});
}

function addSignalOptionsForSelectElems(htmlSelectElemIDsNormal, htmlSelectElemIDsForRoutes) {
	return getSignalsWithCallbacks(
		// success callback
		(responseText) => {
			let responseJson = JSON.parse(responseText);
			// Filter by existance
			htmlSelectElemIDsNormal = htmlSelectElemIDsNormal.filter(selectElemID => $(selectElemID).length);
			htmlSelectElemIDsForRoutes = htmlSelectElemIDsForRoutes.filter(selectElemID => $(selectElemID).length);

			// clear the existing options
			htmlSelectElemIDsNormal.forEach(selectElemID => $(selectElemID).get(0).replaceChildren());
			htmlSelectElemIDsForRoutes.forEach(selectElemID => $(selectElemID).get(0).replaceChildren());

			responseJson.signals.sort((a, b) => {
				let numA = parseInt(a.id.replace(/[^0-9]/g, ''));
				let numB = parseInt(b.id.replace(/[^0-9]/g, ''));
				numA = a.id.includes("platform") ? numA + 100 : numA;
				numB = b.id.includes("platform") ? numB + 100 : numB;
				return numA > numB;
			});

			// add option for each signal for each select
			for (const sig of responseJson.signals) {
				// Omit the platformlights and distant signals for route signal dropdowns
				if (!sig.id.includes("platform") && !sig.id.includes("b")) {
					htmlSelectElemIDsForRoutes.forEach(selectElemID => {
						let optElem = document.createElement("option");
						optElem.text = sig.id;
						$(selectElemID).get(0).add(optElem);
					});
				}
				htmlSelectElemIDsNormal.forEach(selectElemID => {
					let optElem = document.createElement("option");
					optElem.text = sig.id;
					$(selectElemID).get(0).add(optElem);
				});
			}
		},
		(jqXHR) => {
			console.log(`Error reading signals of the SWTbahn platform, code ${jqXHR.status}, text ${jqXHR.responseText}`);
			showErrorInElem("#loadAccessoriesResponse", getErrorMessage(jqXHR));
		}
	);
}

function updateGrantedRoutes(htmlElement) {
	return $.ajax({
		type: 'GET',
		url: '/monitor/granted-routes',
		crossDomain: true,
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			htmlElement.empty();
			if (!responseJson['granted-routes'] || responseJson['granted-routes'].length === 0) {
				htmlElement.html('<li>No granted routes</li>');
				return;
			}
			responseJson['granted-routes'].forEach((route) => {
				const routeId = route['id'];
				const trainId = route.train;
				const routeText = `route ${routeId} granted to ${trainId}`;
				const releaseButton = `<button class="grantedRoute btn smallerLineHeight btn-outline-primary" value=${routeId}>Release</button>`;
				htmlElement.append(`<li>${routeText} ${releaseButton}</li>`);
			});
			$('.grantedRoute').click(function (event) {
				adminReleaseRoute(event.currentTarget.value);
			});
		}
	});
}

// Connectivity 

function pingServer () {
	$('#pingResponse').text('Waiting');
	$.ajax({
		type: 'GET',
		url: '/',
		crossDomain: true,
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#pingResponse", "OK");
		},
		error: function (jqXHR) {
			showErrorInElem("#pingResponse", "Error");
		}
	});
}

// Startup/Shutdown

function startupServer () {
	$('#startupShutdownResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/admin/startup',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#startupShutdownResponse", "Success");
		},
		error: function (jqXHR) {
			showErrorInElem("#startupShutdownResponse", getErrorMessage(jqXHR));
		}
	});
}

function shutdownServer () {
	$('#startupShutdownResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/admin/shutdown',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#startupShutdownResponse", "Success");
			// Reset SessionId and GrabId on shutdown
			sessionId = 0;
			grabId = -1;
			$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
		},
		error: function (jqXHR) {
			showErrorInElem("#startupShutdownResponse", getErrorMessage(jqXHR));
		}
	});
}

// Train Driver

function grabTrain () {
	$('#grabTrainResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/driver/grab-train',
		crossDomain: true,
		data: { 'train': trainId, 'engine': trainEngine },
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			sessionId = responseJson['session-id'];
			grabId = responseJson['grab-id'];
			$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
			showInfoInElem("#grabTrainResponse", "Grabbed");
			updateTrainIsForwards();
		},
		error: function (jqXHR) {
			showErrorInElem("#grabTrainResponse", getErrorMessage(jqXHR));
		}
	});
}

function releaseTrain () {
	$('#grabTrainResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/driver/release-train',
		crossDomain: true,
		data: { 'session-id': sessionId, 'grab-id': grabId },
		dataType: 'text',
		success: function (_responseText) {
			sessionId = 0;
			grabId = -1;
			$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
			showInfoInElem("#grabTrainResponse", "Released");
		},
		error: function (jqXHR) {
			showErrorInElem("#grabTrainResponse", getErrorMessage(jqXHR));
		}
	});
}

function setTrainSpeedDCC (speed) {
	$('#driveTrainResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/driver/set-dcc-train-speed',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId,
			'speed': speed,
			'track-output': trackOutput
		},
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#driveTrainResponse", 'DCC train speed set to ' + speed);
			lastSetSpeed = speed;
		},
		error: function (jqXHR) {
			showErrorInElem("#driveTrainResponse", getErrorMessage(jqXHR));
		}
	});
}

function requestRoute (source, destination) {
	$.ajax({
		type: 'POST',
		url: '/driver/request-route',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId,
			'source': source,
			'destination': destination
		},
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			showInfoInElem("#routeResponse", 
			               'Route ' + responseJson['granted-route-id'] + ' granted');
			$('#routeId').val(responseJson['granted-route-id']);
		},
		error: function (jqXHR) {
			showErrorInElem("#routeResponse", getErrorMessage(jqXHR));
		}
	});
}

function getInterlockerThenRequestRoute (source, destination) {
	$('#routeResponse').text('Waiting');
	$.ajax({
		type: 'GET',
		url: '/controller/get-interlocker',
		crossDomain: true,
		dataType: 'text',
		success: function (_responseText) {
			requestRoute(source, destination);
		},
		error: function (jqXHR) {
			showErrorInElem("#routeResponse", getErrorMessage(jqXHR));
		}
	});
}

function driveRouteIntern (routeId, mode) {
	$.ajax({
		type: 'POST',
		url: '/driver/drive-route',
		crossDomain: true,
		data: {
			'session-id': sessionId,
			'grab-id': grabId,
			'route-id': routeId,
			'mode': mode
		},
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			showInfoInElem("#routeResponse", responseJson['msg']);
			$('#routeId').val("None");
		},
		error: function (jqXHR) {
			showErrorInElem("#routeResponse", getErrorMessage(jqXHR));
		}
	});
}

function driveRoute(routeId, mode) {
	if (isNaN(routeId)) {
		showErrorInElem("#routeResponse", 'Route "' + routeId + '" is not a number!');
	} else if (sessionId != 0 && grabId != -1) {
		driveRouteIntern(routeId, mode);
	} else {
		showErrorInElem("#routeResponse", "No train grabbed!");
	}
}

// Admin

function adminReleaseRoute(routeId) {
	$('#routeId').val(routeId);
	$('#releaseRouteButton').click();
}

function adminSetTrainSpeed (trainId, speed) {
	$.ajax({
		type: 'POST',
		url: '/admin/set-dcc-train-speed',
		crossDomain: true,
		data: {
			'train': trainId,
			'speed': speed,
			'track-output': trackOutput
		},
		dataType: 'text',
		success: function (_responseText) {
			console.log("adminSetTrainSpeed succeeded.");
		},
		error: function (jqXHR) {
			console.log(`adminSetTrainSpeed failed: ${jqXHR.status} - ${getErrorMessage(jqXHR)}`);
		}
	});
}

function adminReleaseTrain (trainId) {
	return $.ajax({
		type: 'POST',
		url: '/admin/release-train',
		crossDomain: true,
		data: {
			'train': trainId
		},
		dataType: 'text',
		success: function (_responseText) {
			console.log("adminReleaseTrain succeeded.");
		},
		error: function (jqXHR) {
			console.log("adminReleaseTrain failed.");
			console.log(`adminReleaseTrain failed: ${jqXHR.status} - ${getErrorMessage(jqXHR)}`);
		}
	});
}

// Controller

function releaseRoute (routeId) {
	$.ajax({
		type: 'POST',
		url: '/controller/release-route',
		crossDomain: true,
		data: { 'route-id': routeId },
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#routeResponse", 'Route ' + routeId + ' released');
			$('#routeId').val("None");
		},
		error: function (jqXHR) {
			showErrorInElem("#routeResponse", getErrorMessage(jqXHR));
		}
	});
}

function setPoint (pointId, pointPosition) {
	$('#setPointResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/controller/set-point',
		crossDomain: true,
		data: { 'point': pointId, 'state': pointPosition },
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#setPointResponse", 'Point ' + pointId + ' set to ' + pointPosition);
		},
		error: function (jqXHR) {
			showErrorInElem("#setPointResponse", getErrorMessage(jqXHR, {404: "Unknown Point"}));
		}
	});
}

function setSignal (signalId, signalAspect) {
	$('#setSignalResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/controller/set-signal',
		crossDomain: true,
		data: { 'signal': signalId, 'state': signalAspect },
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#setSignalResponse", 'Signal ' + signalId + ' set to ' + signalAspect);
		},
		error: function (jqXHR) {
			showErrorInElem("#setSignalResponse", getErrorMessage(jqXHR, {404: "Unknown Signal"}));
		}
	});
}

function setPeripheralState (peripheralId, peripheralAspect) {
	$('#setPeripheralResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/controller/set-peripheral',
		crossDomain: true,
		data: { 'peripheral': peripheralId, 'state': peripheralAspect },
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#setPeripheralResponse", 
			               'Peripheral ' + peripheralId + ' set to ' + peripheralAspect);
		},
		error: function (jqXHR) {
			showErrorInElem("#setPeripheralResponse", getErrorMessage(jqXHR));
		}
	});
}

// Engine and Interlocker

function uploadEngine (file) {
	$('#uploadResponse').text('Waiting');
	var formData = new FormData();
	formData.append('file', file);
	$.ajax({
		type: 'POST',
		url: '/upload/engine',
		crossDomain: true,
		data: formData,
		processData: false,
		contentType: false,
		cache: false,
		dataType: 'text',
		success: function (_responseText) {
			console.log("Upload Success, refreshing engines");
			refreshEnginesList();
			$('#verificationLogDownloadButton').hide();
			$('#clearVerificationMsgButton').hide();
			showInfoInElem("#uploadResponse", 'Engine ' + file.name + ' ready for use');
		},
		error: function (jqXHR) {
			console.log("Engine Upload Failed");
			try {
				var resJson = JSON.parse(jqXHR.responseText, null, 2);
				console.log(resJson);
				var msg = "Server Message: " + resJson["msg"];
				if (resJson.verifiedproperties) {
					msg += "\nList of Properties:"
					resJson.verifiedproperties.forEach(element => {
						msg += "\n-" + element["property"]["name"] + ": " + element["verificationmessage"]
					});
				}
				verificationObj = resJson;
				showErrorInElem("#uploadResponse", msg);
				$('#verificationLogDownloadButton').show();
			} catch (e) {
				console.log("Unable to parse server's reply in upload-engine failure case: " + e);
				showErrorInElem("#uploadResponse", getErrorMessage(jqXHR));
			}
			$('#clearVerificationMsgButton').show();
		}
	});
}

function refreshEnginesList() {
	$('#refreshRemoveEngineResponse').text('Waiting');
	$.ajax({
		type: 'GET',
		url: '/monitor/engines',
		crossDomain: true,
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			var engineList = responseJson['engines'];

			var selectedGrabEngine = $("#grabEngine");
			var selectAvailableEngines = $("#availableEngines");

			selectedGrabEngine.empty();
			selectAvailableEngines.empty();

			$.each(engineList, function (key, value) {
				selectedGrabEngine.append(new Option(value));
				selectAvailableEngines.append(new Option(value));
			});

			showInfoInElem("#refreshRemoveEngineResponse", "Refreshed list of train engines");
		},
		error: function (jqXHR) {
			showErrorInElem("#refreshRemoveEngineResponse", 
			                getErrorMessage(jqXHR, customCode_500ServerUnableReplyMsg));
		}
	});
}

function removeEngine (engineName) {
	$('#refreshRemoveEngineResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/upload/remove-engine',
		crossDomain: true,
		data: { 'engine-name': engineName },
		dataType: 'text',
		success: function (_responseText) {
			console.log("Engine removal successful, now auto-updating available engines.");
			refreshEnginesList();
			showInfoInElem("#refreshRemoveEngineResponse", 'Engine ' + engineName + ' removed');
		},
		error: function (jqXHR) {
			showErrorInElem("#refreshRemoveEngineResponse", getErrorMessage(jqXHR));
		}
	});
}

function uploadInterlocker(file) {
	var formData = new FormData();
	formData.append('file', file);
	$('#uploadResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/upload/interlocker',
		crossDomain: true,
		data: formData,
		processData: false,
		contentType: false,
		cache: false,
		dataType: 'text',
		success: function (_responseText) {
			console.log("Successfuly uploaded interlocker, now refreshing interlocker list.");
			refreshInterlockersList();
			showInfoInElem("#uploadResponse", 'Interlocker ' + file.name + ' ready for use');
		},
		error: function (jqXHR) {
			showErrorInElem("#uploadResponse", getErrorMessage(jqXHR));
		}
	});
}

function refreshInterlockersList() {
	$('#refreshRemoveInterlockerResponse').text('Waiting');
	$.ajax({
		type: 'GET',
		url: '/monitor/interlockers',
		crossDomain: true,
		dataType: 'text',
		success: function (responseText) {
			const responseJson = JSON.parse(responseText);
			var interlockerList = responseJson['interlockers'];

			var selectAvailableInterlockers = $("#availableInterlockers");
			selectAvailableInterlockers.empty();
			$.each(interlockerList, function (key, value) {
				selectAvailableInterlockers.append(new Option(value));
			});

			showInfoInElem("#refreshRemoveInterlockerResponse", "Refreshed list of interlockers");
		},
		error: function (jqXHR) {
			showErrorInElem("#refreshRemoveInterlockerResponse", 
			                'Unable to refresh list of interlockers: ' 
			                + getErrorMessage(jqXHR, customCode_500ServerUnableReplyMsg));
		}
	});
}

function removeInterlocker (interlockerName) {
	$('#refreshRemoveInterlockerResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/upload/remove-interlocker',
		crossDomain: true,
		data: { 'interlocker-name': interlockerName },
		dataType: 'text',
		success: function (_responseText) {
			console.log("Removed interlocker success, now refreshing interlocker list");
			refreshInterlockersList();
			showInfoInElem("#refreshRemoveInterlockerResponse", 
			               'Interlocker ' + interlockerName + ' removed');
		},
		error: function (jqXHR) {
			showErrorInElem("#refreshRemoveInterlockerResponse", getErrorMessage(jqXHR));
		}
	});
}

function setInterlocker (interlockerName) {
	$('#refreshRemoveInterlockerResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/controller/set-interlocker',
		crossDomain: true,
		data: { 'interlocker': interlockerName },
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#refreshRemoveInterlockerResponse", 
			               'Interlocker ' + interlockerName + ' is set');
			console.log("Set Interlocker Success, now refreshing interlocker list");
			refreshInterlockersList();
			showInfoInElem("#interlockerInUse", interlockerName);
		},
		error: function (jqXHR) {
			showErrorInElem("#refreshRemoveInterlockerResponse", getErrorMessage(jqXHR));
		}
	});
}

function unsetInterlocker (interlockerName) {
	$('#refreshRemoveInterlockerResponse').text('Waiting');
	$.ajax({
		type: 'POST',
		url: '/controller/unset-interlocker',
		crossDomain: true,
		data: { 'interlocker': interlockerName },
		dataType: 'text',
		success: function (_responseText) {
			showInfoInElem("#refreshRemoveInterlockerResponse", 
			               'Interlocker ' + interlockerName + ' is unset');
			console.log("Unset interlocker success, refreshing interlocker list");
			refreshInterlockersList();
			showErrorInElem("#interlockerInUse", "No interlocker is set");
		},
		error: function (jqXHR) {
			showErrorInElem("#refreshRemoveInterlockerResponse", getErrorMessage(jqXHR));
		}
	});
}

// Init

$(document).ready(
	function () {
		$('#verificationLogDownloadButton').hide();
		$('#clearVerificationMsgButton').hide();

		$('#pingButton').click(function () {
			pingServer();
		});

		// Startup/Shutdown

		$('#startupButton').click(function () {
			startupServer();
		});

		$('#shutdownButton').click(function () {
			shutdownServer();
		});

		// Loading Accessories

		$('#loadAccessoriesButton').click(function () {
			// IDs match where expected to across the 3 html's (client, controller, driver).
			// The add...Options function takes care of omitting IDs that don't exist on the
			// current page.
			const signalDropDownIDs = ['#signalId'];
			const signalDropDownIDsForRoutes = ['#signalIdFrom', '#signalIdTo'];
			const pointDropDownIDs = ['#pointId'];
			addSignalOptionsForSelectElems(signalDropDownIDs, signalDropDownIDsForRoutes)
				.then(() => addPointOptionsForSelectElems(pointDropDownIDs))
				.then(() => showInfoInElem('#loadAccessoriesResponse', 'Loaded'));
		});

		// Train Driver

		$('#grabTrainButton').click(function () {
			trainId = $('#grabTrainId option:selected').text();
			trainEngine = $('#grabEngine option:selected').text();
			if (sessionId == 0 && grabId == -1) {
				grabTrain();
			} else {
				showErrorInElem("#grabTrainResponse", "You can only grab one train.");
			}
		});

		$('#releaseTrainButton').click(function () {
			if (sessionId != 0 && grabId != -1) {
				releaseTrain();
			} else {
				showErrorInElem("#grabTrainResponse", "No train grabbed!");
			}
		});

		$('#speedMinus').click(function () {
			var speed = $('#dccSpeed').val();
			var position = speeds.indexOf(speed);
			if (position > 0) {
				$('#dccSpeed').val(speeds[position - 1]);
			}
		});

		$('#speedPlus').click(function () {
			var speed = $('#dccSpeed').val();
			var position = speeds.indexOf(speed);
			if (position < speeds.length - 1) {
				$('#dccSpeed').val(speeds[position + 1]);
			}
		});

		$('#swapDirection').click(function () {
			trainIsForwards = !trainIsForwards;
			var enteredSpeed = $('#dccSpeed').val();
			if (lastSetSpeed == 0) {
				// This is done to trigger the train head/rear-lights to switch
				// even if train is currently set to speed 0.
				$('#dccSpeed').val(1);
				$('#driveTrainButton').click();
				$('#dccSpeed').val(0);
				setTimeout(function () {
					$('#driveTrainButton').click();
					// after swapping direction, reinstate speed displayed in `#dccSpeed` before swapping.
					$('#dccSpeed').val(enteredSpeed);
				}, 100 /* milliseconds */);
			} else {
				$('#dccSpeed').val(Math.abs(lastSetSpeed));
				$('#driveTrainButton').click();
				// after swapping direction, reinstate speed displayed in `#dccSpeed` before swapping.
				$('#dccSpeed').val(enteredSpeed);
			}
		});

		$('#driveTrainButton').click(function () {
			var speed = $('#dccSpeed').val();
			speed = trainIsForwards ? speed : -speed;
			if (sessionId != 0 && grabId != -1) {
				setTrainSpeedDCC(speed);
			} else {
				showErrorInElem("#driveTrainResponse", "No train grabbed!");
			}
		});

		$('#stopTrainButton').click(function () {
			var enteredSpeed = $('#dccSpeed').val();
			$('#dccSpeed').val(0);
			$('#driveTrainButton').click();
			// after stopping, reinstate speed displayed in `#dccSpeed` before stopping.
			$('#dccSpeed').val(enteredSpeed);
		});

		$('#requestRouteButton').click(function () {
			let source = $('#signalIdFrom').val();
			let destination = $('#signalIdTo').val();
			if (sessionId != 0 && grabId != -1) {
				getInterlockerThenRequestRoute(source, destination);
			} else {
				showErrorInElem("#routeResponse", "No train grabbed!");
			}
		});

		$('#automaticDriveRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			driveRoute(routeId, "automatic");
		});

		$('#manualDriveRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			driveRoute(routeId, "manual");
		});

		// Verification 

		$('#clearVerificationMsgButton').click(function () {
			$('#verificationLogDownloadButton').hide();
			$('#clearVerificationMsgButton').hide();
			$('#uploadResponse').text('');
			$('#uploadResponse').parent().removeClass('alert-danger');
		});

		$('#verificationLogDownloadButton').click(function () {
			// Create zip file that contains the logs
			// then trigger download of that file.
			try {
				let logList = "";
				verificationObj["verifiedproperties"].forEach(element => {
					logList += atob(element["verificationlog"]) + "\n\n";
				});
				SaveAsFile(logList, "verificationLogs.txt", "text/plain;charset=utf-8");
			} catch (e) {
				console.log(e);
			}
		});

		// Custom Engines
		$('#uploadEngineButton').click(function () {
			var files = $('#selectUploadFile').prop('files');
			if (files.length != 1) {
				showErrorInElem("#uploadResponse", "No SCCharts file selected!");
			} else {
				uploadEngine(files[0]);
			}
		});

		$('#refreshEnginesButton').click(function () {
			refreshEnginesList();
		});

		$('#removeEngineButton').click(function () {
			var engineName = $('#availableEngines option:selected').text();
			if (engineName.search("unremovable") != -1) {
				showErrorInElem("#refreshRemoveEngineResponse", 'Engine ' + engineName + ' is unremovable!');
			} else {
				removeEngine(engineName);
			}
		});

		// Admin control of grabbed trains and train speed
		///TODO: Move this when merging in the the 134-/135-branches.
		const trainIds = [
			'cargo_db',
			'cargo_green',
			'cargo_bayern',
			'cargo_g1000bb',
			'perso_sbb',
			'regional_odeg',
			'regional_brengdirect'
		];

		trainIds.forEach((trainId) => {
			$(`#driveTrainButton_${trainId}`).click(function () {
				const speed = $(`#dccSpeed_${trainId}`).val();
				adminSetTrainSpeed(trainId, speed);
			});

			$(`#releaseTrainButton_${trainId}`).click(function () {
				adminReleaseTrain(trainId);
			});
		});

		// Controller 

		$('#releaseRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			if (isNaN(routeId)) {
				showErrorInElem("#routeResponse", 'Route \"' + routeId + '\" is not a number!');
			} else {
				releaseRoute(routeId);
			}
		});

		$('#setPointButton').click(function () {
			var pointId = $('#pointId').val();
			var pointPosition = $("#pointPosition option:selected").text();
			setPoint(pointId, pointPosition);
		});

		$('#setPointButtonNormal').click(function () {
			var pointId = $('#pointId').val();
			var pointPosition = 'normal';
			setPoint(pointId, pointPosition);
		});

		$('#setPointButtonReverse').click(function () {
			var pointId = $('#pointId').val();
			var pointPosition = 'reverse';
			setPoint(pointId, pointPosition);
		});

		$('#setSignalButton').click(function () {
			var signalId = $('#signalId').val();
			var signalAspect = $("#signalAspect option:selected").text();
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonRed').click(function () {
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_stop';
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonYellow').click(function () {
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_caution';
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonGreen').click(function () {
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_go';
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonWhite').click(function () {
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_shunt';
			setSignal(signalId, signalAspect);
		});

		$('#setPeripheralStateButton').click(function () {
			var peripheralId = $('#peripheralId').val();
			var peripheralAspect = $('#peripheralState').val();
			setPeripheralState(peripheralId, peripheralAspect);
		});

		// Custom Interlockers

		$('#uploadInterlockerButton').click(function () {
			var files = $('#selectUploadFile').prop('files');
			if (files.length != 1) {
				showErrorInElem("#uploadResponse", "No BahnDSL file selected!");
			} else {
				uploadInterlocker(files[0]);
			}
		});

		$('#refreshInterlockersButton').click(function () {
			refreshInterlockersList();
		});

		$('#removeInterlockerButton').click(function () {
			var interlockerName = $('#availableInterlockers option:selected').text();
			if (interlockerName.search("unremovable") != -1) {
				showErrorInElem("#refreshRemoveInterlockerResponse", 
				                'Interlocker ' + interlockerName + ' is unremovable!');
			} else {
				removeInterlocker(interlockerName);
			}
		});

		$('#setInterlockerButton').click(function () {
			var interlockerName = $('#availableInterlockers option:selected').text();
			setInterlocker(interlockerName);
		});

		$('#unsetInterlockerButton').click(function () {
			var interlockerName = $('#interlockerInUse').text();
			unsetInterlocker(interlockerName);
		});

		// File chooser button for Driver and Controller

		$('#selectUploadFile').change(function () {
			$('#selectUploadFileResponse').text(this.files[0].name);
			showInfoInElem("#uploadResponse", 'Selected ' + this.files[0].name);
		});
	}
);

