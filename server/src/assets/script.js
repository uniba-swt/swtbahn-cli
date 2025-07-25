var trackOutput = 'master';
var sessionId = 0;
var grabId = -1;
var trainId = '';
var trainEngine = '';
var verificationObj = {};
var trainIsForwards = true;
var lastSetSpeed = 0;			// Only needed when swapping train direction

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

function getErrorMessage(responseData, customCodes = {}) {
	try {
		if (responseData.responseText.length > 0) {
			respJson = JSON.parse(responseData.responseText);
			if (respJson.msg) {
				return respJson.msg;
			}
		}
	} catch (parseError) {
		console.log("Unable to parse responseData to JSON.");
	}
	// Apparently sometimes we have .responseText, sometimes we don't. I don't know why.
	// 

	// Check if customCodes contains the status code
	if (customCodes.hasOwnProperty(responseData.status)) {
		return customCodes[responseData.status];
	}

	// Fallback to default messages
	switch (responseData.status) {
		case 405:
			return "Method not allowed";
		case 503:
			return "SWTbahn not running";
		default:
			return `${responseData.status} - Error message not defined`;
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
		success: function (responseData) {
			responseJson = JSON.parse(responseData);
			trainIsForwards = responseJson.direction === 'forwards';
		}
	});
}

function updateTrainGrabbedState() {
	// Called from client.html - TODO: Test if the return is needed.
	return $.ajax({
		type: 'GET',
		url: '/monitor/trains',
		crossDomain: true,
		dataType: 'text',
		success: function (responseData) {
			responseJson = JSON.parse(responseData);
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

function updateGrantedRoutes(htmlElement) {
	// Called from client.html - TODO: Test if the return is needed.
	return $.ajax({
		type: 'GET',
		url: '/monitor/granted-routes',
		crossDomain: true,
		dataType: 'text',
		success: function (responseData) {
			responseJson = JSON.parse(responseData);
			htmlElement.empty();
			if (!responseJson['granted-routes'] || responseJson['granted-routes'].length === 0) {
				htmlElement.html('<li>No granted routes</li>');
				return;
			}
			responseJson['granted-routes'].forEach((route) => {
				const routeId = route['id'];
				const trainId = route.train;
				const routeText = `route ${routeId} granted to ${trainId}`;
				const releaseButton = `<button class="grantedRoute" value=${routeId}>Release</button>`;
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#pingResponse", "OK");
		},
		error: function (responseData, textStatus, errorThrown) {
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
		success: function (responseText, textStatus, jqXHR) {
			showInfoInElem("#startupShutdownResponse", "Success");
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#startupShutdownResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#startupShutdownResponse", "Success");
			// Reset SessionId and GrabId on shutdown
			sessionId = 0;
			grabId = -1;
			$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#startupShutdownResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			responseJson = JSON.parse(responseData.responseText);
			sessionId = responseJson['session-id'];
			grabId = responseJson['grab-id'];
			$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
			showInfoInElem("#grabTrainResponse", "Grabbed");
			updateTrainIsForwards();
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#grabTrainResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			sessionId = 0;
			grabId = -1;
			$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
			showInfoInElem("#grabTrainResponse", "Released");
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#grabTrainResponse", getErrorMessage(responseData));
		}
	});
}

function setTrainSpeedDCC () {
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#driveTrainResponse", 'DCC train speed set to ' + speed);
			lastSetSpeed = speed;
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#driveTrainResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			responseJson = JSON.parse(responseData.responseText);
			showInfoInElem("#routeResponse", 
			               'Route ' + responseJson['granted-route-id'] + ' granted');
			$('#routeId').val(responseJson['granted-route-id']);
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#routeResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			requestRoute(source, destination);
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#routeResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			responseJson = JSON.parse(responseData.responseText);
			showInfoInElem("#routeResponse", responseJson['msg']);
			$('#routeId').val("None");
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#routeResponse", getErrorMessage(responseData));
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
		success: (responseData, textStatus, jqXHR) => {
			console.log("adminSetTrainSpeed succeeded.");
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(`adminSetTrainSpeed failed: ${responseData.status} - ${getErrorMessage(responseData)}`);
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
		success: (responseData, textStatus, jqXHR) => {
			console.log("adminReleaseTrain succeeded.");
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log("adminReleaseTrain failed.");
			console.log(`adminReleaseTrain failed: ${responseData.status} - ${getErrorMessage(responseData)}`);
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#routeResponse", 'Route ' + routeId + ' released');
			$('#routeId').val("None");
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#routeResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#setPointResponse", 'Point ' + pointId + ' set to ' + pointPosition);
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#setPointResponse", getErrorMessage(responseData, {404: "Unknown Point"}));
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#setSignalResponse", 'Signal ' + signalId + ' set to ' + signalAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log(responseData);
			console.log(responseData.responseText);
			showErrorInElem("#setSignalResponse", getErrorMessage(responseData, {404: "Unknown Signal"}));
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#setPeripheralResponse", 
			               'Peripheral ' + peripheralId + ' set to ' + peripheralAspect);
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#setPeripheralResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			console.log("Upload Success, refreshing engines");
			refreshEnginesList();
			$('#verificationLogDownloadButton').hide();
			$('#clearVerificationMsgButton').hide();
			showInfoInElem("#uploadResponse", 'Engine ' + file.name + ' ready for use');
		},
		error: function (responseData, textStatus, errorThrown) {
			console.log("Upload Failed");
			try {
				var resJson = JSON.parse(responseData.responseText, null, 2);
				var msg = "Server Message: " + resJson["msg"];
				msg += "\nList of Properties:"
				resJson["verifiedproperties"].forEach(element => {
					msg += "\n-" + element["property"]["name"] + ": " + element["verificationmessage"]
				});
				verificationObj = resJson;
				showErrorInElem("#uploadResponse", msg);
				$('#verificationLogDownloadButton').show();
			} catch (e) {
				console.log("Unable to parse server's reply in upload-engine failure case: " + e);
				showErrorInElem("#uploadResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			responseJson = JSON.parse(responseData.responseText);
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
		error: function (responseData, textStatus, errorThrown) {
			///TODO: Add a custom code for 500 - Server unable to build reply msg
			showErrorInElem("#refreshRemoveEngineResponse", getErrorMessage(responseData, {}));
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
		success: function (responseData, textStatus, jqXHR) {
			console.log("Engine removal successful, now auto-updating available engines.");
			refreshEnginesList();
			showInfoInElem("#refreshRemoveEngineResponse", 'Engine ' + engineName + ' removed');
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#refreshRemoveEngineResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			console.log("Successfuly uploaded interlocker, now refreshing interlocker list.");
			refreshInterlockersList();
			showInfoInElem("#uploadResponse", 'Interlocker ' + file.name + ' ready for use');
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#uploadResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			console.log(responseData);
			console.log(responseData.responseText);
			responseJson = JSON.parse(responseData.responseText);
			var interlockerList = responseJson['interlockers'];

			var selectAvailableInterlockers = $("#availableInterlockers");
			selectAvailableInterlockers.empty();
			$.each(interlockerList, function (key, value) {
				selectAvailableInterlockers.append(new Option(value));
			});

			showInfoInElem("#refreshRemoveInterlockerResponse", "Refreshed list of interlockers");
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#refreshRemoveInterlockerResponse", 
			                'Unable to refresh list of interlockers: ' + getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			console.log("Removed interlocker success, now refreshing interlocker list");
			refreshInterlockersList();
			showInfoInElem("#refreshRemoveInterlockerResponse", 
							'Interlocker ' + interlockerName + ' removed');
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#refreshRemoveInterlockerResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#refreshRemoveInterlockerResponse", 
			               'Interlocker ' + interlockerName + ' is set');
			console.log("Set Interlocker Success, now refreshing interlocker list");
			refreshInterlockersList();
			showInfoInElem("#interlockerInUse", interlockerName);
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#refreshRemoveInterlockerResponse", getErrorMessage(responseData));
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
		success: function (responseData, textStatus, jqXHR) {
			showInfoInElem("#refreshRemoveInterlockerResponse", 
			               'Interlocker ' + interlockerName + ' is unset');
			console.log("Unset interlocker success, refreshing interlocker list");
			refreshInterlockersList();
			showErrorInElem("#interlockerInUse", "No interlocker is set");
		},
		error: function (responseData, textStatus, errorThrown) {
			showErrorInElem("#refreshRemoveInterlockerResponse", getErrorMessage(responseData));
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
			speed = $('#dccSpeed').val();
			position = speeds.indexOf(speed);
			if (position > 0) {
				$('#dccSpeed:text').val(speeds[position - 1]);
			}
		});

		$('#speedPlus').click(function () {
			speed = $('#dccSpeed').val();
			position = speeds.indexOf(speed);
			if (position < speeds.length - 1) {
				$('#dccSpeed:text').val(speeds[position + 1]);
			}
		});

		$('#swapDirection').click(function () {
			trainIsForwards = !trainIsForwards;
			enteredSpeed = $('#dccSpeed').val();
			if (lastSetSpeed == 0) {
				// This is done to trigger the train head/rear-lights to switch
				$('#dccSpeed').val(1);
				$('#driveTrainButton').click();
				$('#dccSpeed').val(0);
				setTimeout(function () {
					$('#driveTrainButton').click();
					$('#dccSpeed').val(enteredSpeed);
				}, 100 /* milliseconds */);
			} else {
				$('#dccSpeed').val(Math.abs(lastSetSpeed));
				$('#driveTrainButton').click();
				$('#dccSpeed').val(enteredSpeed);
			}
		});

		$('#driveTrainButton').click(function () {
			speed = $('#dccSpeed').val();
			speed = trainIsForwards ? speed : -speed;
			if (sessionId != 0 && grabId != -1) {
				setTrainSpeedDCC();
			} else {
				showErrorInElem("#driveTrainResponse", "No train grabbed!");
			}
		});

		$('#stopTrainButton').click(function () {
			enteredSpeed = $('#dccSpeed').val();
			$('#dccSpeed').val(0);
			$('#driveTrainButton').click();
			$('#dccSpeed').val(enteredSpeed);
		});

		$('#requestRouteButton').click(function () {
			source = $('#signalIdFrom').val();
			destination = $('#signalIdTo').val();
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
				logList = "";
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

