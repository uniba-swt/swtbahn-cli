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
			responseJson = JSON.parse(responseData);
			htmlElement.empty();
			if (!responseJson.granted-routes || responseJson.granted-routes.length === 0) {
				htmlElement.html('<li>No granted routes</li>');
				return;
			}
			responseJson.granted-routes.forEach((route) => {
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
					$('#startupShutdownResponse').text('Success');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#startupShutdownResponse').parent().removeClass('alert-success');
					$('#startupShutdownResponse').parent().addClass('alert-danger');
					$('#startupShutdownResponse').text(getErrorMessage(responseData));
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
					// Reset SessionId and GrabId on shutdown
					sessionId = 0;
					grabId = -1;
					$('#sessionGrabId')
						.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#startupShutdownResponse').parent().removeClass('alert-success');
					$('#startupShutdownResponse').parent().addClass('alert-danger');
					$('#startupShutdownResponse').text(getErrorMessage(responseData));
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
						responseJson = JSON.parse(responseData);
						sessionId = responseJson['session-id'];
						grabId = responseJson['grab-id'];
						$('#sessionGrabId')
							.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
						$('#grabTrainResponse').parent().removeClass('alert-danger');
						$('#grabTrainResponse').parent().addClass('alert-success');
						$('#grabTrainResponse').text('Grabbed');
						updateTrainIsForwards();
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#grabTrainResponse').parent().removeClass('alert-success');
						$('#grabTrainResponse').parent().addClass('alert-danger');
						$('#grabTrainResponse').text(getErrorMessage(responseData));
					}
				});
			} else {
				$('#grabTrainResponse').parent().removeClass('alert-success');
				$('#grabTrainResponse').parent().addClass('alert-danger');
				$('#grabTrainResponse').text('You can only grab one train!');
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
						$('#sessionGrabId').text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
						$('#grabTrainResponse').parent().removeClass('alert-danger');
						$('#grabTrainResponse').parent().addClass('alert-success');
						$('#grabTrainResponse').text('Released grabbed train');
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#grabTrainResponse').parent().removeClass('alert-success');
						$('#grabTrainResponse').parent().addClass('alert-danger');
						$('#grabTrainResponse').text(getErrorMessage(responseData));
					}
				});
			} else {
				$('#grabTrainResponse').parent().removeClass('alert-success');
				$('#grabTrainResponse').parent().addClass('alert-danger');
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
						$('#driveTrainResponse').parent().removeClass('alert-danger');
						$('#driveTrainResponse').parent().addClass('alert-success');
						$('#driveTrainResponse').text('DCC train speed set to ' + speed);
						lastSetSpeed = speed;
					},
					error: function (responseData, textStatus, errorThrown) { 
						$('#driveTrainResponse').parent().removeClass('alert-success');
						$('#driveTrainResponse').parent().addClass('alert-danger');
						$('#driveTrainResponse').text(getErrorMessage(responseData));
					}
				});
			} else {
				$('#driveTrainResponse').parent().removeClass('alert-success');
				$('#driveTrainResponse').parent().addClass('alert-danger');
				$('#driveTrainResponse').text('You must have a grabbed train!');
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
								responseJson = JSON.parse(responseData);
								$('#routeResponse').parent().removeClass('alert-danger');
								$('#routeResponse').parent().addClass('alert-success');
								$('#routeResponse')
									.text('Route ' + responseJson['granted-route-id'] + ' granted');
								$('#routeId').val(responseJson['granted-route-id']);
							},
							error: function (responseData, textStatus, errorThrown) {
								$('#routeResponse').parent().removeClass('alert-success');
								$('#routeResponse').parent().addClass('alert-danger');
								$('#routeResponse').text(getErrorMessage(responseData));
							}
						});
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#routeResponse').parent().removeClass('alert-success');
						$('#routeResponse').parent().addClass('alert-danger');
						$('#routeResponse').text(getErrorMessage(responseData));
					}
				});
			} else {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('You must have a grabbed train!');
			}
		});

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
						responseJson = JSON.parse(responseData);
						$('#routeResponse').parent().removeClass('alert-danger');
						$('#routeResponse').parent().addClass('alert-success');
						$('#routeResponse').text(responseJson['msg']);
						$('#routeId').val("None");
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#routeResponse').parent().removeClass('alert-success');
						$('#routeResponse').parent().addClass('alert-danger');
						$('#routeResponse').text(getErrorMessage(responseData));
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

		$('#manualDriveRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			driveRoute(routeId, "manual");
		});

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
					$('#uploadResponse').parent().removeClass('alert-success');
					$('#uploadResponse').parent().addClass('alert-danger');
					try {
						var resJson = JSON.parse(responseData, null, 2);
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
					responseJson = JSON.parse(responseData);
					var engineList = responseJson['engines'];

					var selectedGrabEngine = $("#grabEngine");
					var selectAvailableEngines = $("#availableEngines");

					selectedGrabEngine.empty();
					selectAvailableEngines.empty();

					$.each(engineList, function (key, value) {
						selectedGrabEngine.append(new Option(value));
						selectAvailableEngines.append(new Option(value));
					});

					$('#refreshRemoveEngineResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-success');
					$('#refreshRemoveEngineResponse').text('Refreshed list of train engines');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveEngineResponse').parent().removeClass('alert-success');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-danger');
					///TODO: Add a custom code for 500 - Server unable to build reply msg
					$('#refreshRemoveEngineResponse').text(getErrorMessage(responseData));
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
					$('#refreshRemoveEngineResponse').parent().removeClass('alert-danger');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-success');
					$('#refreshRemoveEngineResponse').text('Engine ' + engineName + ' removed');
					console.log("Engine removal successful, now auto-updating available engines.");
					refreshEnginesList();
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#refreshRemoveEngineResponse').parent().removeClass('alert-success');
					$('#refreshRemoveEngineResponse').parent().addClass('alert-danger');
					$('#refreshRemoveEngineResponse').text(getErrorMessage(responseData));
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
					console.log("adminSetTrainSpeed succeeded.");
				},
				error: (responseData, textStatus, errorThrown) => {
					console.log(`adminSetTrainSpeed failed: ${responseData.status} - ${getErrorMessage(responseData)}`);
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
					console.log("adminReleaseTrain succeeded.");
				},
				error: (responseData, textStatus, errorThrown) => {
					console.log("adminReleaseTrain failed.");
					console.log(`adminReleaseTrain failed: ${responseData.status} - ${getErrorMessage(responseData)}`);
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

		function releaseRoute(routeId) {
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
		}
		
		$('#releaseRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').val();
			if (isNaN(routeId)) {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('Route \"' + routeId + '\" is not a number!');
				return;
			}
			releaseRoute(routeId);
		});


		function setPoint(pointId, pointPosition) {
			$.ajax({
				type: 'POST',
				url: '/controller/set-point',
				crossDomain: true,
				data: { 'point': pointId, 'state': pointPosition },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#setPointResponse').parent().removeClass('alert-danger');
					$('#setPointResponse').parent().addClass('alert-success');
					$('#setPointResponse')
						.text('Point ' + pointId + ' set to ' + pointPosition);
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#setPointResponse').parent().removeClass('alert-success');
					$('#setPointResponse').parent().addClass('alert-danger');
					$('#setPointResponse').text(getErrorMessage(responseData));
				}
			});
		}

		$('#setPointButton').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = $("#pointPosition option:selected").text();
			setPoint(pointId, pointPosition);
		});

		$('#setPointButtonNormal').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = 'normal';
			setPoint(pointId, pointPosition);
		});

		$('#setPointButtonReverse').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = 'reverse';
			setPoint(pointId, pointPosition);
		});


		function setSignal(signalId, signalAspect) {
			$.ajax({
				type: 'POST',
				url: '/controller/set-signal',
				crossDomain: true,
				data: { 'signal': signalId, 'state': signalAspect },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#setSignalResponse').parent().removeClass('alert-danger');
					$('#setSignalResponse').parent().addClass('alert-success');
					$('#setSignalResponse')
						.text('Signal ' + signalId + ' set to ' + signalAspect);
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#setSignalResponse').parent().removeClass('alert-success');
					$('#setSignalResponse').parent().addClass('alert-danger');
					$('#setSignalResponse').text(getErrorMessage(responseData));
				}
			});
		}

		$('#setSignalButton').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = $("#signalAspect option:selected").text();
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonRed').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_stop';
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonYellow').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_caution';
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonGreen').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_go';
			setSignal(signalId, signalAspect);
		});

		$('#setSignalButtonWhite').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'aspect_shunt';
			setSignal(signalId, signalAspect);
		});


		function setPeripheralState(peripheralId, peripheralAspect) {
			$.ajax({
				type: 'POST',
				url: '/controller/set-peripheral',
				crossDomain: true,
				data: { 'peripheral': peripheralId, 'state': peripheralAspect },
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					$('#setPeripheralResponse').parent().removeClass('alert-danger');
					$('#setPeripheralResponse').parent().addClass('alert-success');
					$('#setPeripheralResponse')
						.text('Peripheral ' + peripheralId + ' set to ' + peripheralAspect);
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#setPeripheralResponse').parent().removeClass('alert-success');
					$('#setPeripheralResponse').parent().addClass('alert-danger');
					$('#setPeripheralResponse').text(getErrorMessage(responseData));
				}
			});
		}

		$('#setPeripheralStateButton').click(function () {
			$('#setPeripheralResponse').text('Waiting');
			var peripheralId = $('#peripheralId').val();
			var peripheralAspect = $('#peripheralState').val();
			setPeripheralState(peripheralId, peripheralAspect);
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
					$('#uploadResponse').parent().removeClass('alert-danger');
					$('#uploadResponse').parent().addClass('alert-success');
					$('#uploadResponse')
						.text('Interlocker ' + file.name + ' ready for use');
					console.log("Successfuly uploaded interlocker, now refreshing interlocker list.");
					refreshInterlockersList();
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
					responseJson = JSON.parse(responseData);
					var interlockerList = responseJson['interlockers'];

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
					$('#refreshRemoveInterlockerResponse')
						.text('Unable to refresh list of interlockers: ' + getErrorMessage(responseData));
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
				$('#refreshRemoveInterlockerResponse')
					.text('Interlocker ' + interlockerName + ' is unremovable!');
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
					$('#interlockerInUse').text('No interlocker is set.');
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
			$('#uploadResponse').parent().removeClass('alert-danger');
			$('#uploadResponse').parent().addClass('alert-success');
			$('#uploadResponse').text('Selected ' + this.files[0].name);
		});
	
	//#*# Helperfunctions

	function getErrorMessage(responseData, customCodes = {}) {
		respJson = JSON.parse(responseData);
		if (respJson.msg) {
			return respJson.msg;
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

