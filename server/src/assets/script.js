var trackOutput = 'master';
var sessionId = 0;
var grabId = -1;
var trainId = '';
var trainEngine = '';

$(document).ready(
	function () {
		trackOutput = 'master';

		// Configuration
		$('#pingButton').click(function () {
			$('#pingResponse').text('Waiting');
			$.ajax({
				type: 'POST',
				url: '/',
				crossDomain: true,
				data: null,
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
					$('#startupShutdownResponse').parent().removeClass('alert-success');
					$('#startupShutdownResponse').parent().addClass('alert-danger');
					$('#startupShutdownResponse').text('System already running!');
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
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#startupShutdownResponse').parent().removeClass('alert-success');
					$('#startupShutdownResponse').parent().addClass('alert-danger');
					$('#startupShutdownResponse').text('System not running!');
				}
			});
		});


		// Train Driver
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
						responseDataSplit = responseData.split(',');
						sessionId = responseDataSplit[0];
						grabId = responseDataSplit[1];
						$('#sessionGrabId')
							.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
						$('#grabTrainResponse').text('Grabbed');
						$('#grabTrainResponse').parent().removeClass('alert-danger');
						$('#grabTrainResponse').parent().addClass('alert-success');
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#grabTrainResponse')
							.text('System not running or train not available!');
						$('#grabTrainResponse').parent().addClass('alert-danger');
						$('#grabTrainResponse').parent().removeClass('alert-success');
					}
				});
			} else {
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
						$('#sessionGrabId')
							.text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
						$('#grabTrainResponse').text('Released grabbed train');
						$('#grabTrainResponse').parent().removeClass('alert-danger');
						$('#grabTrainResponse').parent().addClass('alert-success');
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#grabTrainResponse').parent().addClass('alert-danger');
						$('#grabTrainResponse').parent().removeClass('alert-success');
						$('#grabTrainResponse').text('System not running or train still moving!');
					}
				});
			} else {
				$('#grabTrainResponse').parent().addClass('alert-danger');
				$('#grabTrainResponse').parent().removeClass('alert-success');
				$('#grabTrainResponse').text('No grabbed train!');
			}
		});

		speeds = [
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

		$('#driveTrainButton').click(function () {
			$('#driveTrainResponse').text('Waiting');
			speed = $('#dccSpeed').val();
			direction = $('#direction').is(':checked');
			speed = direction ? speed : -speed;
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
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#driveTrainResponse')
							.text('System not running or invalid track output!');
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
			lastSpeed = $('#dccSpeed').val();
			$('#dccSpeed').val(0);
			$('#driveTrainButton').click();
			$('#dccSpeed').val(lastSpeed);
		});

		$('#requestRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			source = $('#signalIdFrom').val();
			destination = $('#signalIdTo').val();
			if (sessionId != 0 && grabId != -1) {
				$.ajax({
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
								$('#routeResponse').parent().removeClass('alert-danger');
								$('#routeResponse').parent().addClass('alert-success');
								$('#routeResponse').text('Route ' + responseData + ' granted');

								$('#routeId').text(responseData);
							},
							error: function (responseData, textStatus, errorThrown) {
								$('#routeResponse').parent().removeClass('alert-success');
								$('#routeResponse').parent().addClass('alert-danger');
								$('#routeResponse').text(responseData.responseText);
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

				ajaxGetInterlocker().then(ajaxRequestRoute);
				
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
					$('routeId').text(routeId);
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

		$('#driveRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			if (sessionId != 0 && grabId != -1) {
				var routeId = $('#routeId').text();
				$.ajax({
					type: 'POST',
					url: '/driver/drive-route',
					crossDomain: true,
					data: { 'session-id': sessionId, 'grab-id': grabId, 'route-id': routeId },
					dataType: 'text',
					success: function (responseData, textStatus, jqXHR) {
						$('#routeResponse').parent().removeClass('alert-danger');
						$('#routeResponse').parent().addClass('alert-success');
						$('#routeResponse')
							.text('Route ' + responseData + ' driving completed');
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#routeResponse').parent().removeClass('alert-success');
						$('#routeResponse').parent().addClass('alert-danger');
						$('#routeResponse').text('Route could not be driven!');
					}
				});
			} else {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('You must have a grabbed train!');
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
					refreshEnginesList();
					$('#uploadResponse').parent().removeClass('alert-danger');
					$('#uploadResponse').parent().addClass('alert-success');
					$('#uploadResponse')
						.text('Engine ' + file.name + ' ready for use');
				},
				error: function (responseData, textStatus, errorThrown) {
					$('#uploadResponse').parent().removeClass('alert-success');
					$('#uploadResponse').parent().addClass('alert-danger');
					$('#uploadResponse').text(responseData.responseText);
				}
			});
		});

		function refreshEnginesList() {
			$.ajax({
				type: 'POST',
				url: '/upload/refresh-engines',
				crossDomain: true,
				data: null,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					var engineList = responseData.split(",");

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
					$('#refreshRemoveEngineResponse').text(responseData.responseText);
				}
			});
		});


		// Controller
		
		// For driver.html
		$('#releaseRouteButton').click(function () {
			$('#routeResponse').text('Waiting');
			var routeId = $('#routeId').text();
			if (isNaN(routeId)) {
				$('#routeResponse').parent().removeClass('alert-success');
				$('#routeResponse').parent().addClass('alert-danger');
				$('#routeResponse').text('Route \"' + routeId + '\" is not a number!');
			} else {
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
					
						$('#routeId').text("None");
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#routeResponse').parent().removeClass('alert-success');
						$('#routeResponse').parent().addClass('alert-danger');
						$('#routeResponse')
							.text('System not running or invalid track output!');
					}
				});
			}
		});
		
		// For client.html
		$('#releaseRouteButtonClient').click(function () {
			$('#releaseRouteResponse').text('Waiting');
			var routeId = $('#routeId').val();
			if (isNaN(routeId)) {
				$('#releaseRouteResponse').parent().removeClass('alert-success');
				$('#releaseRouteResponse').parent().addClass('alert-danger');
				$('#releaseRouteResponse').text('Route \"' + routeId + '\" is not a number!');
			} else {
				$.ajax({
					type: 'POST',
					url: '/controller/release-route',
					crossDomain: true,
					data: { 'route-id': routeId },
					dataType: 'text',
					success: function (responseData, textStatus, jqXHR) {
						$('#releaseRouteResponse').parent().removeClass('alert-danger');
						$('#releaseRouteResponse').parent().addClass('alert-success');
						$('#releaseRouteResponse').text('Route ' + routeId + ' released');
					
						$('#routeId').text("None");
					},
					error: function (responseData, textStatus, errorThrown) {
						$('#releaseRouteResponse').parent().removeClass('alert-success');
						$('#releaseRouteResponse').parent().addClass('alert-danger');
						$('#releaseRouteResponse')
							.text('System not running or invalid track output!');
					}
				});
			}
		});

		points = [
			'point1',
			'point2',
			'point3',
			'point4',
			'point5',
			'point6',
			'point7',
			'point8',
			'point9',
			'point10',
			'point11',
			'point12',
		]

		$('#pointMinus').click(function () {
			point = $('#pointId').val();
			position = points.indexOf(point);
			if (position > 0) {
				$('#pointId:text').val(points[position - 1]);
			}
		});

		$('#pointPlus').click(function () {
			point = $('#pointId').val();
			position = points.indexOf(point);
			if (position < points.length - 1) {
				$('#pointId:text').val(points[position + 1]);
			}
		});

		$('#setPointButton').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = $("#pointPosition option:selected").text();

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
					$('#setPointResponse').text('System not running or invalid position!');
					$('#setPointResponse').parent().removeClass('alert-success');
					$('#setPointResponse').parent().addClass('alert-danger');
				}
			});
		});

		$('#setPointButtonNormal').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = 'normal';

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
					$('#setPointResponse').text('System not running or invalid position!');
					$('#setPointResponse').parent().removeClass('alert-success');
					$('#setPointResponse').parent().addClass('alert-danger');
				}
			});
		});

		$('#setPointButtonReverse').click(function () {
			$('#setPointResponse').text('Waiting');
			var pointId = $('#pointId').val();
			var pointPosition = 'reverse';

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
					$('#setPointResponse').text('System not running or invalid position!');
					$('#setPointResponse').parent().removeClass('alert-success');
					$('#setPointResponse').parent().addClass('alert-danger');
				}
			});
		});

		signals = [
			'signal1',
			'signal2',
			'signal3',
			'signal4',
			'signal5',
			'signal6',
			'signal7',
			'signal8',
			'signal9',
			'signal10',
			'signal11',
			'signal12',
			'signal13',
			'signal14',
			'signal15',
			'signal16',
			'signal17',
			'signal18',
			'signal19',
			'platformlights'
		]

		$('#signalMinusFrom').click(function () {
			signal = $('#signalIdFrom').val();
			position = signals.indexOf(signal);
			if (position > 0) {
				$('#signalIdFrom:text').val(signals[position - 1]);
			}
		});

		$('#signalMinusTo').click(function () {
			signal = $('#signalIdTo').val();
			position = signals.indexOf(signal);
			if (position > 0) {
				$('#signalIdTo:text').val(signals[position - 1]);
			}
		});

		$('#signalPlusFrom').click(function () {
			signal = $('#signalIdFrom').val();
			position = signals.indexOf(signal);
			if (position < signals.length - 1) {
				$('#signalIdFrom:text').val(signals[position + 1]);
			}
		});

		$('#signalPlusTo').click(function () {
			signal = $('#signalIdTo').val();
			position = signals.indexOf(signal);
			if (position < signals.length - 1) {
				$('#signalIdTo:text').val(signals[position + 1]);
			}
		});

		$('#signalMinus').click(function () {
			signal = $('#signalId').val();
			position = signals.indexOf(signal);
			if (position > 0) {
				$('#signalId:text').val(signals[position - 1]);
			}
		});

		$('#signalPlus').click(function () {
			signal = $('#signalId').val();
			position = signals.indexOf(signal);
			if (position < signals.length - 1) {
				$('#signalId:text').val(signals[position + 1]);
			}
		});

		$('#setSignalButton').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = $("#signalAspect option:selected").text();
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
					$('#setSignalResponse').text('System not running or invalid aspect!');
					$('#setSignalResponse').parent().removeClass('alert-success');
					$('#setSignalResponse').parent().addClass('alert-danger');
				}
			});
		});


		$('#setSignalButtonRed').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'red';
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
					$('#setSignalResponse').text('System not running or invalid aspect!');
					$('#setSignalResponse').parent().removeClass('alert-success');
					$('#setSignalResponse').parent().addClass('alert-danger');
				}
			});
		});

		$('#setSignalButtonYellow').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'yellow';
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
					$('#setSignalResponse').text('System not running or invalid aspect!');
					$('#setSignalResponse').parent().removeClass('alert-success');
					$('#setSignalResponse').parent().addClass('alert-danger');
				}
			});
		});

		$('#setSignalButtonGreen').click(function () {
			$('#setSignalResponse').text('Waiting');
			var signalId = $('#signalId').val();
			var signalAspect = 'green';
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
					$('#setSignalResponse').text('System not running or invalid aspect!');
					$('#setSignalResponse').parent().removeClass('alert-success');
					$('#setSignalResponse').parent().addClass('alert-danger');
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
					$('#uploadResponse').text(responseData.responseText);
				}
			});
		});

		function refreshInterlockersList() {
			$.ajax({
				type: 'POST',
				url: '/upload/refresh-interlockers',
				crossDomain: true,
				data: null,
				dataType: 'text',
				success: function (responseData, textStatus, jqXHR) {
					var interlockerList = responseData.split(",");

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
					$('#refreshRemoveInterlockerResponse').text(responseData.responseText);
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
					$('#refreshRemoveInterlockerResponse').text('Unable to set interlocker');
				}
			});
		});

		$('#unsetInterlockerButton').click(function () {
			$('#refreshRemoveInterlockerResponse').text('Waiting');
			var interlockerName = $('#selecedInterlocker').text();
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
					$('#refreshRemoveInterlockerResponse').text('Unable to unset interlocker');
				}
			});
		});

		// File chooser button for Driver and Controller
		$('#selectUploadFile').change(function () {
			$('#selectUploadFileResponse').text(this.files[0].name);

			$('#uploadResponse').text('Selected SCCharts model ' + this.files[0].name);
			$('#uploadResponse').parent().removeClass('alert-danger');
			$('#uploadResponse').parent().addClass('alert-success');
		});

	}
);








