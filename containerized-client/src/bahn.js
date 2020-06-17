/** @class BahnAPI providing an interface to the HTTP API of SWT Bahn */
class BahnAPI {

    /**
     * @constructor
     * @param {string} baseUrl The base URL of the API. Empty string uses the root of this server.
     */
    constructor(baseUrl = "") {
        this.baseUrl = baseUrl;
    }

    /**
     * Logs a user in.
     *
     * @param {string} username
     * @param {string} password
     * @returns {Promise<boolean>} A promise eventually resolving to the logged in state for this request.
     */
    async login(username, password) {
        this.username = username;
        this.password = password;

        // ping server to check if the provided credentials are correct
        let response = await this._fetch('/user/info');
        let userInfo = await response.json();

        console.log(userInfo);

        if (this._login_callback) {
            this._login_callback(userInfo);
        }

        return userInfo.loggedIn;
    }

    login_callback(callback) {
        this._login_callback = callback;
    }

    /**
     * Logs the user out.
     */
    logout() {
        this.username = null;
        this.password = null;

        if (this._logout_callback) {
            this._logout_callback();
        }
    }

    logout_callback(callback) {
        this._logout_callback = callback;
    }

    /**
     *
     * @returns {Promise<Response>}
     */
    async ping() {
        return this._fetch('/');
    }

    /**
     *
     * @returns {Promise<string>}
     */
    async startup() {
        let response = await this._fetch('/admin/startup', 'POST');

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return 'failed'; // TODO: wouldn't it make much more sense to let the server return some actual error message and use that to help the user?
        }
    }

    /**
     *
     * @returns {Promise<string>}
     */
    async shutdown() {
        let response = await this._fetch('/admin/shutdown', 'POST');

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return 'failed';
        }
    }

    async getAllUsers() {
        let response = await this._fetch('/user/all');

        if (response.status === 200) {
            return response.json();
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async deactivateUser(user) {
        let body = new FormData();
        body.append('user', user);
        let response = await this._fetch('/user/deactivate', 'POST', body);

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async activateUser(user) {
        let body = new FormData();
        body.append('user', user);
        let response = await this._fetch('/user/activate', 'POST', body);

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async addRole(user, role) {
        let body = new FormData();
        body.append('user', user);
        body.append('role', role);
        let response = await this._fetch('/user/role/add', 'POST', body);

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async removeRole(user, role) {
        let body = new FormData();
        body.append('user', user);
        body.append('role', role);
        let response = await this._fetch('/user/role/remove', 'POST', body);

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async deleteUser(user) {
        let body = new FormData();
        body.append('user', user);
        let response = await this._fetch('/user/delete', 'POST', body);

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async registerUser(username, password, email) {
        let body = new FormData();
        body.append('user', username);
        body.append('password', password);
        body.append('mail', email);
        let response = await this._fetch('/user/register', 'POST', body);

        if (response.status === 200) {
            return 'ok';
        } else if (response.status === 401) {
            return 'unauthorized';
        } else {
            return await response.text();
        }
    }

    async _fetch(path, method = 'GET', body = null) {
        let headers = new Headers();

        if (this.username && this.password) {
            headers.set('Authorization', 'Basic ' + window.btoa(this.username + ':' + this.password));
        }

        return fetch(this.baseUrl + path, {
            method: method,
            headers: headers,
            body: body
        })
    }
}