pipeline {
    agent { label "devel-image && x86_64" }

    parameters {
        booleanParam name: 'RUN_MEMCHECKS', defaultValue: true, description: 'Run memchecks?'

        // Extra for Eaton CI
        // Use DEFAULT_DEPLOY_BRANCH_PATTERN and DEFAULT_DEPLOY_JOB_NAME if
        // defined in this jenkins setup -- in Jenkins Management Web-GUI
        // see Configure System / Global properties / Environment variables
        // Default (if unset) is empty => no deployment attempt after good test
        string (
            defaultValue: '${DEFAULT_DEPLOY_BRANCH_PATTERN}',
            description: 'Regular expression of branch names for which a deploy action would be attempted after a successful build and test; leave empty to not deploy. Reasonable value is ^(master|release/.*|feature/*)$',
            name : 'DEPLOY_BRANCH_PATTERN')
        string (
            defaultValue: '${DEFAULT_DEPLOY_JOB_NAME}',
            description: 'Name of your job that handles deployments and should accept arguments: DEPLOY_GIT_URL DEPLOY_GIT_BRANCH DEPLOY_GIT_COMMIT -- and it is up to that job what to do with this knowledge (e.g. git archive + push to packaging); leave empty to not deploy',
            name : 'DEPLOY_JOB_NAME')
        booleanParam (
            defaultValue: true,
            description: 'If the deployment is done, should THIS job wait for it to complete and include its success or failure as the build result (true), or should it schedule the job and exit quickly to free up the executor (false)',
            name: 'DEPLOY_REPORT_RESULT')

        booleanParam (
            defaultValue: true,
            description: 'When using temporary subdirs in build/test workspaces, wipe them after the whole job is done successfully?',
            name: 'DO_CLEANUP_AFTER_JOB')
        booleanParam (
            defaultValue: false,
            description: 'When using temporary subdirs in build/test workspaces, wipe them after the whole job is done unsuccessfully (failed)? Note this would not allow postmortems on CI server, but would conserve its disk space.',
            name: 'DO_CLEANUP_AFTER_FAILED_JOB')
    }

    stages {
        stage('Build') {
            steps {
                cmakeBuild buildType: 'Release',
                cleanBuild: true,
                installation: 'InSearchPath',
                steps: [[withCmake: true]]
            }
        }

        stage('Tests') {
            steps {
                cmakeBuild buildType: 'Release',
                cleanBuild: true,
                installation: 'InSearchPath',
                steps: [[args: 'test']]
            }
        }

        stage('Memchecks') {
            when {
                environment name: 'RUN_MEMCHECKS', value: 'true'
            }
            steps {
                cmakeBuild buildType: 'Release',
                cleanBuild: true,
                installation: 'InSearchPath',
                steps: [[args: 'test memcheck']]
            }
        }

        stage ('deploy if appropriate') {
            steps {
                script {
                    def myDEPLOY_JOB_NAME = sh(returnStdout: true, script: """echo "${params["DEPLOY_JOB_NAME"]}" """).trim();
                    def myDEPLOY_BRANCH_PATTERN = sh(returnStdout: true, script: """echo "${params["DEPLOY_BRANCH_PATTERN"]}" """).trim();
                    def myDEPLOY_REPORT_RESULT = sh(returnStdout: true, script: """echo "${params["DEPLOY_REPORT_RESULT"]}" """).trim().toBoolean();
                    echo "Original: DEPLOY_JOB_NAME : ${params["DEPLOY_JOB_NAME"]} DEPLOY_BRANCH_PATTERN : ${params["DEPLOY_BRANCH_PATTERN"]} DEPLOY_REPORT_RESULT : ${params["DEPLOY_REPORT_RESULT"]}"
                    echo "Used:     myDEPLOY_JOB_NAME:${myDEPLOY_JOB_NAME} myDEPLOY_BRANCH_PATTERN:${myDEPLOY_BRANCH_PATTERN} myDEPLOY_REPORT_RESULT:${myDEPLOY_REPORT_RESULT}"
                    if ( (myDEPLOY_JOB_NAME != "") && (myDEPLOY_BRANCH_PATTERN != "") ) {
                        if ( env.BRANCH_NAME =~ myDEPLOY_BRANCH_PATTERN ) {
                            def GIT_URL = sh(returnStdout: true, script: """git remote -v | egrep '^origin' | awk '{print \$2}' | head -1""").trim()
                            def GIT_COMMIT = sh(returnStdout: true, script: 'git rev-parse --verify HEAD').trim()
                            def DIST_ARCHIVE = ""
                            def msg = "Would deploy ${GIT_URL} ${GIT_COMMIT} because tested branch '${env.BRANCH_NAME}' matches filter '${myDEPLOY_BRANCH_PATTERN}'"
                            if ( params.DO_DIST_DOCS ) {
                                DIST_ARCHIVE = env.BUILD_URL + "artifact/__dist.tar.gz"
                                msg += ", using dist archive '${DIST_ARCHIVE}' to speed up deployment"
                            }
                            echo msg
                            milestone ordinal: 100, label: "${env.JOB_NAME}@${env.BRANCH_NAME}"
                            build job: "${myDEPLOY_JOB_NAME}", parameters: [
                                string(name: 'DEPLOY_GIT_URL', value: "${GIT_URL}"),
                                string(name: 'DEPLOY_GIT_BRANCH', value: env.BRANCH_NAME),
                                string(name: 'DEPLOY_GIT_COMMIT', value: "${GIT_COMMIT}"),
                                string(name: 'DEPLOY_DIST_ARCHIVE', value: "${DIST_ARCHIVE}")
                                ], quietPeriod: 0, wait: myDEPLOY_REPORT_RESULT, propagate: myDEPLOY_REPORT_RESULT
                        } else {
                            echo "Not deploying because branch '${env.BRANCH_NAME}' did not match filter '${myDEPLOY_BRANCH_PATTERN}'"
                        }
                    } else {
                        echo "Not deploying because deploy-job parameters are not set"
                    }
                }
            }
        }
    }
    post {
        success {
            script {
                if (currentBuild.getPreviousBuild()?.result != 'SUCCESS') {
                    // Uncomment desired notification

                    //slackSend (color: "#008800", message: "Build ${env.JOB_NAME} is back to normal.")
                    //emailext (to: "qa@example.com", subject: "Build ${env.JOB_NAME} is back to normal.", body: "Build ${env.JOB_NAME} is back to normal.")
                }
                if ( params.DO_CLEANUP_AFTER_JOB ) {
                    dir("tmp") {
                        deleteDir()
                    }
                }
            }
        }
        failure {
            // Uncomment desired notification
            // Section must not be empty, you can delete the sleep once you set notification
            sleep 1
            //slackSend (color: "#AA0000", message: "Build ${env.BUILD_NUMBER} of ${env.JOB_NAME} ${currentBuild.result} (<${env.BUILD_URL}|Open>)")
            //emailext (to: "qa@example.com", subject: "Build ${env.JOB_NAME} failed!", body: "Build ${env.BUILD_NUMBER} of ${env.JOB_NAME} ${currentBuild.result}\nSee ${env.BUILD_URL}")

            dir("tmp") {
                script {
                    if ( params.DO_CLEANUP_AFTER_FAILED_JOB ) {
                        deleteDir()
                    } else {
                        sh """ echo "NOTE: BUILD AREA OF WORKSPACE `pwd` REMAINS FOR POST-MORTEMS ON `hostname` AND CONSUMES `du -hs . | awk '{print \$1}'` !" """
                    }
                }
            }
        }
    }

}
