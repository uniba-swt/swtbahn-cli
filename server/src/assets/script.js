var url = '';
var trackOutput = 'master';
var sessionId = 0;
var grabId = -1;
var trainId = '';
var trainEngine = '';
var routeId = -1;
$(document).ready(function() {

        $('#configButton').click(function() {
            $('#configResponse').text('Waiting');
            trackOutput = $('#trackOutput').val();
            $('#configResponse').text('OK');
        });

        $('#pingButton').click(function() {
            $('#pingResponse').text('Waiting');
            $.ajax({
                type: 'POST',
                url: url,
                crossDomain: true,
                data: null,
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#pingResponse').text('OK');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#pingResponse').text('Error');
                }
            });
        });

        $('#startupButton').click(function() {
            $('#startupResponse').text('Waiting');
            $.ajax({
                type: 'POST',
                url: url + '/admin/startup',
                crossDomain: true,
                data: null,
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#startupResponse').text('OK');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#startupResponse').text('System already running!');
                }
            });
        });

        $('#shutdownButton').click(function() {
            $('#shutdownResponse').text('Waiting');
            $.ajax({
                type: 'POST',
                url: url + '/admin/shutdown',
                crossDomain: true,
                data: null,
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#shutdownResponse').text('OK');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#shutdownResponse').text('System not running!');
                }
            });
        });

        $('#grabTrainButton').click(function() {
            $('#grabTrainResponse').text('Waiting');
            trainId = $('#grabTrainId option:selected').text();
            trainEngine = $('#grabEngine option:selected').text();
            if (sessionId == 0 && grabId == -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/grab-train',
                    crossDomain: true,
                    data: { 'train': trainId, 'engine': trainEngine },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        responseDataSplit = responseData.split(',');
                        sessionId = responseDataSplit[0];
                        grabId = responseDataSplit[1];
                        $('#sessionGrabId')
                            .text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
                        $('#grabTrainResponse').text('Grabbed');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#grabTrainResponse')
                            .text('System not running or train not available!');
                    }
                });
            } else {
                $('#grabTrainResponse').text('You can only grab one train!');
            }
        });

        $('#releaseTrainButton').click(function() {
            $('#grabTrainResponse').text('Waiting');
            if (sessionId != 0 && grabId != -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/release-train',
                    crossDomain: true,
                    data: { 'session-id': sessionId, 'grab-id': grabId },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        sessionId = 0;
                        grabId = -1;
                        $('#sessionGrabId')
                            .text('Session ID: ' + sessionId + ', Grab ID: ' + grabId);
                        $('#grabTrainResponse').text('Released grabbed train');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#grabTrainResponse').text('System not running!');
                    }
                });
            } else {
                $('#grabTrainResponse').text('No grabbed train!');
            }
        });

        $('#driveTrainButton').click(function() {
            $('#driveTrainResponse').text('Waiting');
            speed = $('#dccSpeed').val();
            direction = $('#direction').is(':checked');
            speed = direction ? speed : -speed;
            if (sessionId != 0 && grabId != -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/set-dcc-train-speed',
                    crossDomain: true,
                    data: {
                        'session-id': sessionId,
                        'grab-id': grabId,
                        'speed': speed,
                        'track-output': trackOutput
                    },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        $('#driveTrainResponse').text('DCC train speed set to ' + speed);
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#driveTrainResponse')
                            .text('System not running or invalid track output!');
                    }
                });
            } else {
                $('#driveTrainResponse').text('You must have a grabbed train!');
            }
        });

        $('#stopTrainButton').click(function() {
            $('#dccSpeed').val(0);
            $('#driveTrainButton').click();
        });

        $('#requestRouteButton').click(function() {
            $('#routeResponse').text('Waiting');
            source = $('#source').val();
            destination = $('#destination').val();
            if (sessionId != 0 && grabId != -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/request-route',
                    crossDomain: true,
                    data: {
                        'session-id': sessionId,
                        'grab-id': grabId,
                        'source': source,
                        'destination': destination
                    },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        routeId = responseData;
                        $('#routeResponse').text('Route ' + responseData + ' granted');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#routeResponse').text('Route not granted!');
                    }
                });
            } else {
                $('#routeResponse').text('You must have a grabbed train!');
            }
        });

        $('#driveRouteButton').click(function() {
            $('#routeResponse').text('Waiting');
            if (sessionId != 0 && grabId != -1) {
                $.ajax({
                    type: 'POST',
                    url: url + '/driver/drive-route',
                    crossDomain: true,
                    data: { 'session-id': sessionId, 'grab-id': grabId, 'route-id': routeId },
                    dataType: 'text',
                    success: function(responseData, textStatus, jqXHR) {
                        $('#routeResponse')
                            .text('Route ' + responseData + ' driving completed');
                    },
                    error: function(responseData, textStatus, errorThrown) {
                        $('#routeResponse').text('Route could not be driven!');
                    }
                });
            } else {
                $('#routeResponse').text('You must have a grabbed train!');
            }
        });

        $('#releaseRouteButton').click(function() {
            $('#releaseRouteResponse').text('Waiting');
            var routeId = $('#routeId').val();
            $.ajax({
                type: 'POST',
                url: url + '/controller/release-route',
                crossDomain: true,
                data: { 'route-id': routeId },
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#releaseRouteResponse').text('Route ' + routeId + ' released');
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#releaseRouteResponse')
                        .text('System not running or invalid track output!');
                }
            });
        });

        $('#setPointButton').click(function() {
            $('#setPointResponse').text('Waiting');
            var pointId = $('#pointId').val();
            var pointPosition = $('#pointPosition option:selected').text();
            $.ajax({
                type: 'POST',
                url: url + '/controller/set-point',
                crossDomain: true,
                data: { 'point': pointId, 'state': pointPosition },
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#setPointResponse')
                        .text('Point ' + pointId + ' set to ' + pointPosition);
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#setPointResponse').text('System not running or invalid position!');
                }
            });
        });

        $('#setSignalButton').click(function() {
            $('#setSignalResponse').text('Waiting');
            var signalId = $('#signalId').val();
            var signalAspect = $('#signalAspect option:selected').text();
            $.ajax({
                type: 'POST',
                url: url + '/controller/set-signal',
                crossDomain: true,
                data: { 'signal': signalId, 'state': signalAspect },
                dataType: 'text',
                success: function(responseData, textStatus, jqXHR) {
                    $('#setSignalResponse')
                        .text('Signal ' + signalId + ' set to ' + signalAspect);
                },
                error: function(responseData, textStatus, errorThrown) {
                    $('#setSignalResponse').text('System not running or invalid aspect!');
                }
            });
        });
    }

);