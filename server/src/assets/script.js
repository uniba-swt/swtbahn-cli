var trackOutput = 'master';
var sessionId = 0;
var grabId = -1;
var trainId = '';
var trainEngine = '';
var verificationObj = {};
var trainIsForwards = true;
var lastSetSpeed = 0;			// Only needed when swapping train direction

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
	return $.ajax({
		type: 'GET',
		url: '/monitor/trains',
		crossDomain: true,
		dataType: 'text',
		success: function (responseData) {
			responseData.trains.forEach((train) => {
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




// Admin control of granted routes
function adminReleaseRoute(routeId) {
	$('#routeId').val(routeId);
	$('#releaseRouteButton').click();
}

function updateGrantedRoutes(htmlElement) { //TODO: to be tested if responseData.granted-routes works or responseData["granted-routes"]
	return $.ajax({
		type: 'GET',
		url: '/monitor/granted-routes',
		crossDomain: true,
		dataType: 'text',
		success: function (responseData) {
			htmlElement.empty();
			if (!responseData.granted-routes || responseData.granted-routes.length === 0) {
				htmlElement.html('<li>No granted routes</li>');
				return;
			}
			responseData.granted-routes.forEach((route) => {
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


$(document).ready(
	function () {
		$('#verificationLogDownloadButton').hide();
		$('#clearVerificationMsgButton').hide();

		// Configuration
		$('#pingButton').click(function () {
			$('#pingResponse').text('Waiting');
			$.ajax({
				type: 'GET',
				url: '/',
				crossDomain: true,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#pingResponse').parent().removeClass('alert-danger');
					$('#pingResponse').parent().addClass('alert-success');
					$('#pingResponse').text('OK');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#pingResponse').parent().removeClass('alert-success');
					$('#pingResponse').parent().addClass('alert-danger');
					$('#pingResponse').text('Error');
				}
			});
		});


		$('#startupButton').click(function () {
			$('#startupShutdownResponse').text('Waiting');
			$.ajax({
				type: 'POST',
				url: '/admin/startup',
				crossDomain: true,
				data: null,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#startupShutdownResponse').parent().removeClass('alert-danger');
					$('#startupShutdownResponse').parent().addClass('alert-success');
					$('#startupShutdownResponse').text('OK');
				},
				error: function (responseData, textStatus, errorThrown) {
					responseJson = JSON.parse(responseData);
					$('#startupShutdownResponse').parent().removeClass('alert-success');
					$('#startupShutdownResponse').parent().addClass('alert-danger');
					$('#startupShutdownResponse').text(getErrorMessage(responseJson));
				}
			});
		});

		$('#shutdownButton').click(function () {
			$('#startupShutdownResponse').text('Waiting');
			$.ajax({
				type: 'POST',
				url: '/admin/shutdown',
				crossDomain: true,
				data: null,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#startupShutdownResponse').parent().removeClass('alert-danger');
					$('#startupShutdownResponse').parent().addClass('alert-success');
					$('#startupShutdownResponse').text('OK');

					sessionId = 0;
					grabId = -1;
					$('#sessionGrabId')
						.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#startupShutdownResponse').parent().removeClass('alert-success');
					$('#startupShutdownResponse').parent().addClass('alert-danger');
					$('#startupShutdownResponse').text(getErrorMessage(JSON.parse(responseData)));
				}
			});
		});

		$('#grabTrainButton').click(function () {
			$('#grabTrainResponse').text('Waiting');
			trainId = $('#grabTrainId option:selected').text();
			trainEngine = $('#grabEngine option:selected').text();
			if (sessionId == 0 && grabId == -1) {
				$.ajax({
					type: 'POST',
					url: '/driver/grab-train',
					crossDomain: true,
					data: { 'train': trainId, 'engine': trainEngine },
					dataType: 'text',
					success: function (responseData, textStatus, jqXHR) {
						sessionId = responseData['session-id'];
						grabId = responseData['grab-id'];
						$('#sessionGrabId')
							.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
						$('#grabTrainResponse').text('Grabbed');
						$('#grabTrainResponse').parent().removeClass('alert-danger');
						$('#grabTrainResponse').parent().addClass('alert-success');
						updateTrainIsForwards();
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#grabTrainResponse')
							.text(getErrorMessage(responseData));
						$('#grabTrainResponse').parent().addClass('alert-danger');
						$('#grabTrainResponse').parent().removeClass('alert-success');
					}
				});
			} else {
				$('#grabTrainResponse').text('You can only grab one train!');
				$('#grabTrainResponse').parent().addClass('alert-danger');
				$('#grabTrainResponse').parent().removeClass('alert-success');
			}
		});
		$('#releaseTrainButton').click(function () {
			$('#grabTrainResponse').text('Waiting');
			if (sessionId != 0 && grabId != -1) {
				$.ajax({
					type: 'POST',
					url: '/driver/release-train',
					crossDomain: true,
					data: { 'session-id': sessionId, 'grab-id': grabId },
					dataType: 'text',
					success: function (responseData, textStatus, jqXHR) {
						sessionId = 0;
						grabId = -1;
						$('#sessionGrabId')
							.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
						$('#grabTrainResponse').text('Released grabbed train');
						$('#grabTrainResponse').parent().removeClass('alert-danger');
						$('#grabTrainResponse').parent().addClass('alert-success');
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#grabTrainResponse').parent().addClass('alert-danger');
						$('#grabTrainResponse').parent().removeClass('alert-success');
						$('#grabTrainResponse').text(getErrorMessage(responseData));
					}
				});
			} else {
				$('#grabTrainResponse').parent().addClass('alert-danger');
				$('#grabTrainResponse').parent().removeClass('alert-success');
				$('#grabTrainResponse').text('No grabbed train!');
			}
		});

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
			$('#driveTrainResponse').text('Waiting');
			speed = $('#dccSpeed').val();
			speed = trainIsForwards ? speed : -speed;
			if (sessionId != 0 && grabId != -1) {
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
						$('#driveTrainResponse').text('DCC train speed set to ' + speed);
						$('#driveTrainResponse').parent().removeClass('alert-danger');
						$('#driveTrainResponse').parent().addClass('alert-success');
						lastSetSpeed = speed;
					},
					error: function (responseData, textStatus, errorThrown) { //TODO: Code 405 and 503 doesnt have any msg
						var statusCode = responseData.status;
						$('#driveTrainResponse')
							.text(getErrorMessage(responseData));
						
							$('#driveTrainResponse').parent().addClass('alert-danger');
						$('#driveTrainResponse').parent().removeClass('alert-success');
					}
				});
			} else {
				$('#driveTrainResponse').text('You must have a grabbed train!');
				$('#driveTrainResponse').parent().addClass('alert-danger');
				$('#driveTrainResponse').parent().removeClass('alert-success');
			}
		});


		$('#stopTrainButton').click(function () {
			enteredSpeed = $('#dccSpeed').val();
			$('#dccSpeed').val(0);
			$('#driveTrainButton').click();
			$('#dccSpeed').val(enteredSpeed);
		});

		$('#requestRouteButton').click(function () { //TODO: cleanup function
			$('#routeResponse').text('Waiting');
			source = $('#signalIdFrom').val();
			destination = $('#signalIdTo').val();
			if (sessionId != 0 && grabId != -1) {
				$.ajax({
					type: 'GET',
					url: '/controller/get-interlocker',
					crossDomain: true,
					dataType: 'text',
					success: function (responseData, textStatus, jqXHR) {
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
								$('#routeResponse').text('Route ' + responseData['granted-route-id'] + ' granted');
								$('#routeResponse').parent().removeClass('alert-danger');
								$('#routeResponse').parent().addClass('alert-success');

								$('#routeId').val(responseData['granted-route-id']);
							},
							error: function (responseData, textStatus, errorThrown) {
								$('#routeResponse').text(getErrorMessage(responseData));
								$('#routeResponse').parent().removeClass('alert-success');
								$('#routeResponse').parent().addClass('alert-danger');
							}
						});
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#routeResponse').parent().removeClass('alert-success');
						$('#routeResponse').parent().addClass('alert-danger');
						$('#routeResponse').text("No interlocker set!");
					}
				});
			} else {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('You must have a grabbed train!');
			}
		});

		/* $('#requestRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			source = $('#signalIdFrom').val();
			destination = $('#signalIdTo').val();
			if (sessionId != 0 && grabId != -1) {
				ajaxGetInterlocker()
					.then(ajaxRequestRoute);
			} else {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('You must have a grabbed train!');
			}
		});

		function ajaxGetInterlocker(){
			return $.ajax({
				type: 'POST',
				url: '/controller/get-interlocker',
				crossDomain: true,
				data: {
					'session-id': sessionId,
					'grab-id': grabId,
					'source': source,
					'destination': destination
				},
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#requestRouteButton').removeClass('btn-outline-danger');
					$('#requestRouteButton').addClass('btn-outline-primary');

				},
				error: function (responseData, textStatus, errorThrown) {
					$('#routeResponse').parent().removeClass('alert-success');
					$('#routeResponse').parent().addClass('alert-danger');
					$('#routeResponse').text("No interlocker set!");
				}
			});
		}

		function ajaxRequestRoute(responseData, textStatus, jqXHR){
			return $.ajax({
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
					routeId = responseData;
					$('routeId').val(routeId);
					$('#routeResponse').parent().removeClass('alert-danger');
					$('#routeResponse').parent().addClass('alert-success');
					$('#routeResponse').text('Route ' + responseData + ' granted');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#routeResponse').parent().removeClass('alert-success');
					$('#routeResponse').parent().addClass('alert-danger');
					$('#routeResponse').text(responseData.responseText);
				}
			});
		} */

		//From https://github.com/eligrey/FileSaver.js/wiki/FileSaver.js-Example
		function SaveAsFile(content, filename, contentTypeOptions) {
			try {
				var b = new Blob([content], { type: contentTypeOptions });
				saveAs(b, filename);
			} catch (e) {
				console.log("SaveAsFile Failed to save the file. Error msg: " + e);
			}
		}

		function driveRoute(routeId, mode) {
			if (isNaN(routeId)) {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('Route "' + routeId + '" is not a number!');
				return;
			}

			if (sessionId != 0 && grabId != -1) {
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
						$('#routeResponse').text(responseData.msg);
						$('#routeResponse').parent().removeClass('alert-danger');
						$('#routeResponse').parent().addClass('alert-success');
						$('#routeId').val("None");
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#routeResponse').text(getErrorMessage(responseData));
						$('#routeResponse').parent().removeClass('alert-success');
						$('#routeResponse').parent().addClass('alert-danger');
					}
				});
			} else {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('You must have a grabbed train!');
			}
		}

		$('#automaticDriveRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			driveRoute(routeId, "automatic");
		});

		$('#clearVerificationMsgButton').click(function () {
			$('#verificationLogDownloadButton').hide();
			$('#clearVerificationMsgButton').hide();
			$('#uploadResponse').text('');
			$('#uploadResponse').parent().removeClass('alert-danger');
		});

		$('#verificationLogDownloadButton').click(function () {
			//Create zip file that contains the logs
			//then trigger download of that file.
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

		$('#manualDriveRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			driveRoute(routeId, "manual");
		});


		// Custom Engines
		$('#uploadEngineButton').click(function () { //TODO: change to json
			$('#uploadResponse').text('Waiting');
			var files = $('#selectUploadFile').prop('files');
			if (files.length != 1) {
				$('#uploadResponse').parent().removeClass('alert-success');
				$('#uploadResponse').parent().addClass('alert-danger');
				$('#uploadResponse').text('Select an SCCharts file!');
				return;
			}

			var file = files[0];
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
					console.log("Upload Success");
					refreshEnginesList();
					$('#uploadResponse').parent().removeClass('alert-danger');
					$('#uploadResponse').parent().addClass('alert-success');
					$('#uploadResponse').text('Engine ' + file.name + ' ready for use');
					$('#verificationLogDownloadButton').hide();
					$('#clearVerificationMsgButton').hide();
				},
				error: function (responseData, textStatus, errorThrown) {
					console.log("Upload Failed");
					try {
						var resJson = JSON.parse(responseData.responseText.toString(), null, 2);
						var msg = "Server Message: " + resJson["msg"];
						msg += "\nList of Properties:"
						resJson["verifiedproperties"].forEach(element => {
							msg += "\n-" + element["property"]["name"] + ": " + element["verificationmessage"]
						});
						verificationObj = resJson;
						$('#uploadResponse').text(msg);
						$('#verificationLogDownloadButton').show();
					} catch (e) {
						console.log("Unable to parse server's reply in upload-engine failure case: " + e);
						$('#uploadResponse').text(responseData.responseText.toString());
					}
					$('#uploadResponse').parent().removeClass('alert-success');
					$('#uploadResponse').parent().addClass('alert-danger');
					$('#clearVerificationMsgButton').show();
				}
			});
		});

		function refreshEnginesList() {
			$.ajax({
				type: 'GET',
				url: '/monitor/engines',
				crossDomain: true,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					var engineList = responseData.engines;

					var selectGrabEngines = $("#grabEngine");
					var selectAvailableEngines = $("#availableEngines");

					selectGrabEngines.empty();
					selectAvailableEngines.empty();

					$.each(engineList, function (key, value) {
						selectGrabEngines.append(new Option(value));
						selectAvailableEngines.append(new Option(value));
					});

					$('#refreshRemoveEngineResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-success');
					$('#refreshRemoveEngineResponse').text('Refreshed list of train engines');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveEngineResponse').parent().removeClass('alert-success');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-danger');
					$('#refreshRemoveEngineResponse').text('Unable to refresh list of train engines');
				}
			});
		}



		$('#refreshEnginesButton').click(function () {
			$('#refreshRemoveEngineResponse').text('Waiting');
			refreshEnginesList();
		});

		$('#removeEngineButton').click(function () {
			$('#refreshRemoveEngineResponse').text('Waiting');
			var engineName = $('#availableEngines option:selected').text();
			if (engineName.search("unremovable") != -1) {
				$('#refreshRemoveEngineResponse').parent().removeClass('alert-success');
				$('#refreshRemoveEngineResponse').parent().addClass('alert-danger');
				$('#refreshRemoveEngineResponse').text('Engine ' + engineName + ' is unremovable!');
				return;
			}
			$.ajax({
				type: 'POST',
				url: '/upload/remove-engine',
				crossDomain: true,
				data: { 'engine-name': engineName },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					refreshEnginesList();
					$('#refreshRemoveEngineResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-success');
					$('#refreshRemoveEngineResponse').text('Engine ' + engineName + ' removed');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveEngineResponse').parent().removeClass('alert-success');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-danger');
					$('#refreshRemoveEngineResponse').text(getErrorMessage(responseData, {400: "Invalid or missing parameter, or engine is unremovable"}));
				}
			});
		});


		// Admin control of grabbed trains and train speed

		function adminSetTrainSpeed(trainId, speed) {
			return $.ajax({
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
					// Do nothing
				},
				error: (responseData, textStatus, errorThrown) => {
					// Do nothing
					console.log(`func(adminSetTrainSpeed) failed: ${responseData.status} - ${getErrorMessage(responseData)}`);
				}
			});
		}


		function adminReleaseTrain(trainId) {
			return $.ajax({
				type: 'POST',
				url: '/admin/release-train',
				crossDomain: true,
				data: {
					'train': trainId
				},
				dataType: 'text',
				success: (responseData, textStatus, jqXHR) => {
				},
				error: (responseData, textStatus, errorThrown) => {
				}
			});
		}


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
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('Route \"' + routeId + '\" is not a number!');

				return;
			}
			$.ajax({
				type: 'POST',
				url: '/controller/release-route',
				crossDomain: true,
				data: { 'route-id': routeId },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#routeResponse').parent().removeClass('alert-danger');
					$('#routeResponse').parent().addClass('alert-success');
					$('#routeResponse').text('Route ' + routeId + ' released');
					$('#routeId').val("None");
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#routeResponse').parent().removeClass('alert-success');
					$('#routeResponse').parent().addClass('alert-danger');
					$('#routeResponse').text(getErrorMessage(responseData));
				}
			});

		});

		function setPointAjax(pointId, pointPosition) {
			$.ajax({
				type: 'POST',
				url: '/controller/set-point',
				crossDomain: true,
				data: { 'point': pointId, 'state': pointPosition },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#setPointResponse')
						.text('Point ' + pointId + ' set to ' + pointPosition);
					$('#setPointResponse').parent().removeClass('alert-danger');
					$('#setPointResponse').parent().addClass('alert-success');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#setPointResponse').text(getErrorMessage(responseData));
					$('#setPointResponse').parent().removeClass('alert-success');
					$('#setPointResponse').parent().addClass('alert-danger');
				}
			});
		}


		$('#setPointButton').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = $("#pointPosition option:selected").text();
			setPointAjax(pointId, pointPosition);
		});

		$('#setPointButtonNormal').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = 'normal';
			setPointAjax(pointId, pointPosition);
		});

		$('#setPointButtonReverse').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = 'reverse';
			setPointAjax(pointId, pointPosition);
		});

		function setSignalAjax(signalId, signalAspect) {
			$.ajax({
				type: 'POST',
				url: '/controller/set-signal',
				crossDomain: true,
				data: { 'signal': signalId, 'state': signalAspect },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#setSignalResponse')
						.text('Signal ' + signalId + ' set to ' + signalAspect);
					$('#setSignalResponse').parent().removeClass('alert-danger');
					$('#setSignalResponse').parent().addClass('alert-success');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#setSignalResponse').text(getErrorMessage(responseData));
					$('#setSignalResponse').parent().removeClass('alert-success');
					$('#setSignalResponse').parent().addClass('alert-danger');
				}
			});
		}


		$('#setSignalButton').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = $("#signalAspect option:selected").text();
			setSignalAjax(signalId, signalAspect);
		});

		$('#setSignalButtonRed').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_stop';
			setSignalAjax(signalId, signalAspect);
		});

		$('#setSignalButtonYellow').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_caution';
			setSignalAjax(signalId, signalAspect);
		});

		$('#setSignalButtonGreen').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_go';
			setSignalAjax(signalId, signalAspect);
		});

		$('#setSignalButtonWhite').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_shunt';
			setSignalAjax(signalId, signalAspect);
		});

		$('#setPeripheralStateButton').click(function () {
			$('#setPeripheralResponse').text('Waiting');
			var peripheralId = $('#peripheralId').val();
			var peripheralAspect = $('#peripheralState').val();
			$.ajax({
				type: 'POST',
				url: '/controller/set-peripheral',
				crossDomain: true,
				data: { 'peripheral': peripheralId, 'state': peripheralAspect },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#setPeripheralResponse')
						.text('Peripheral ' + peripheralId + ' set to ' + peripheralAspect);
					$('#setPeripheralResponse').parent().removeClass('alert-danger');
					$('#setPeripheralResponse').parent().addClass('alert-success');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#setPeripheralResponse').text(getErrorMessage(responseData));
					$('#setPeripheralResponse').parent().removeClass('alert-success');
					$('#setPeripheralResponse').parent().addClass('alert-danger');
				}
			});
		});



		// Custom Interlockers
		$('#uploadInterlockerButton').click(function () {
			$('#uploadResponse').text('Waiting');
			var files = $('#selectUploadFile').prop('files');
			if (files.length != 1) {
				$('#uploadResponse').parent().removeClass('alert-success');
				$('#uploadResponse').parent().addClass('alert-danger');
				$('#uploadResponse').text('Select a BahnDSL file!');
				return;
			}
			var file = files[0];
			var formData = new FormData();
			formData.append('file', file);
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
					refreshInterlockersList();
					$('#uploadResponse').parent().removeClass('alert-danger');
					$('#uploadResponse').parent().addClass('alert-success');
					$('#uploadResponse')
						.text('Interlocker ' + file.name + ' ready for use');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#uploadResponse').parent().removeClass('alert-success');
					$('#uploadResponse').parent().addClass('alert-danger');
					$('#uploadResponse').text(getErrorMessage(responseData));
				}
			});
		});

		function refreshInterlockersList() {
			$.ajax({
				type: 'GET',
				url: '/monitor/interlockers',
				crossDomain: true,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					var interlockerList = responseData.interlockers;

					var selectAvailableInterlockers = $("#availableInterlockers");

					selectAvailableInterlockers.empty();

					$.each(interlockerList, function (key, value) {
						selectAvailableInterlockers.append(new Option(value));
					});

					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-success');
					$('#refreshRemoveInterlockerResponse').text('Refreshed list of interlockers');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-success');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').text('Unable to refresh list of interlockers');
				}
			});
		}


		$('#refreshInterlockersButton').click(function () {
			$('#refreshRemoveInterlockerResponse').text('Waiting');
			refreshInterlockersList();
		});

		$('#removeInterlockerButton').click(function () {
			$('#refreshRemoveInterlockerResponse').text('Waiting');
			var interlockerName = $('#availableInterlockers option:selected').text();
			if (interlockerName.search("unremovable") != -1) {
				$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-success');
				$('#refreshRemoveInterlockerResponse').parent().addClass('alert-danger');
				$('#refreshRemoveInterlockerResponse').text('Interlocker ' + interlockerName + ' is unremovable!');
				return;
			}
			$.ajax({
				type: 'POST',
				url: '/upload/remove-interlocker',
				crossDomain: true,
				data: { 'interlocker-name': interlockerName },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					refreshInterlockersList();
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-success');
					$('#refreshRemoveInterlockerResponse')
						.text('Interlocker ' + interlockerName + ' removed');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-success');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').text(getErrorMessage(responseData));
				}
			});
		});

		$('#setInterlockerButton').click(function () {
			$('#refreshRemoveInterlockerResponse').text('Waiting');
			var interlockerName = $('#availableInterlockers option:selected').text();
			$.ajax({
				type: 'POST',
				url: '/controller/set-interlocker',
				crossDomain: true,
				data: { 'interlocker': interlockerName },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					refreshInterlockersList();
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-success');
					$('#refreshRemoveInterlockerResponse')
						.text('Interlocker ' + interlockerName + ' is set');

					$('#interlockerInUse').parent().removeClass('alert-danger');
					$('#interlockerInUse').parent().addClass('alert-success');
					$('#interlockerInUse').text(interlockerName);
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-success');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').text(getErrorMessage(responseData));
				}
			});
		});

		$('#unsetInterlockerButton').click(function () {
			$('#refreshRemoveInterlockerResponse').text('Waiting');
			var interlockerName = $('#interlockerInUse').text();
			$.ajax({
				type: 'POST',
				url: '/controller/unset-interlocker',
				crossDomain: true,
				data: { 'interlocker': interlockerName },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					refreshInterlockersList();
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-success');
					$('#refreshRemoveInterlockerResponse')
						.text('Interlocker ' + interlockerName + ' is unset');

					$('#interlockerInUse').parent().removeClass('alert-success');
					$('#interlockerInUse').parent().addClass('alert-danger');
					$('#interlockerInUse').text('No interlocker set!');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveInterlockerResponse').parent().removeClass('alert-success');
					$('#refreshRemoveInterlockerResponse').parent().addClass('alert-danger');
					$('#refreshRemoveInterlockerResponse').text(getErrorMessage(responseData));
				}
			});
		});



		// File chooser button for Driver and Controller
		$('#selectUploadFile').change(function () {
			$('#selectUploadFileResponse').text(this.files[0].name);

			$('#uploadResponse').text('Selected ' + this.files[0].name);
			$('#uploadResponse').parent().removeClass('alert-danger');
			$('#uploadResponse').parent().addClass('alert-success');
		});
	
	//#*# Helperfunctions

	function getErrorMessage(responseData, customCodes = {}) {
		if (responseData.msg) {
			return responseData.msg;
		}

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

	}
);

